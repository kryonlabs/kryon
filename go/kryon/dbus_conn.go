//go:build linux

package kryon

// Minimal D-Bus client over the session bus: EXTERNAL auth, method calls
// with replies, signal subscriptions and serving incoming method calls —
// exactly what the notification and tray integrations need, nothing more.

import (
	"bufio"
	"encoding/binary"
	"errors"
	"fmt"
	"log"
	"net"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"
)

const (
	dbusMsgMethodCall = 1
	dbusMsgReturn     = 2
	dbusMsgError      = 3
	dbusMsgSignal     = 4
)

type dbusMessage struct {
	typ         byte
	flags       byte
	serial      uint32
	path        string
	iface       string
	member      string
	errName     string
	destination string
	sender      string
	sig         string
	replyTo     uint32
	body        []byte
}

type dbusHandlerFunc func(m *dbusMessage) (string, []any, *dbusError)

type dbusError struct {
	name string
	text string
}

func (e *dbusError) Error() string { return e.name + ": " + e.text }

type dbusConn struct {
	c    net.Conn
	r    *bufio.Reader
	wmu  sync.Mutex
	rmu  sync.Mutex // guards writes' counterpart: serial + pending maps
	next uint32

	pendingMu sync.Mutex
	pending   map[uint32]chan *dbusMessage

	handlerMu sync.Mutex
	handlers  map[string]dbusHandlerFunc // "interface.member"

	signalMu sync.Mutex
	signals  []func(*dbusMessage)

	closeOnce sync.Once
	closed    chan struct{}
}

// dbusSession connects to the user's session bus and completes the
// handshake (EXTERNAL auth + Hello).
func dbusSession() (*dbusConn, error) {
	addr := os.Getenv("DBUS_SESSION_BUS_ADDRESS")
	if addr == "" {
		return nil, errors.New("dbus: DBUS_SESSION_BUS_ADDRESS not set")
	}
	transport, rest, ok := strings.Cut(addr, ":")
	if !ok {
		return nil, fmt.Errorf("dbus: bad address %q", addr)
	}
	var (
		c   net.Conn
		err error
	)
	switch transport {
	case "unix":
		var path, abstract string
		for _, kv := range strings.Split(rest, ",") {
			k, v, ok := strings.Cut(kv, "=")
			if !ok {
				continue
			}
			switch k {
			case "path":
				path = v
			case "abstract":
				abstract = v
			}
		}
		switch {
		case path != "":
			c, err = net.Dial("unix", path)
		case abstract != "":
			c, err = net.Dial("unix", "@"+abstract)
		default:
			err = errors.New("dbus: unix address without path")
		}
	default:
		err = fmt.Errorf("dbus: unsupported transport %q", transport)
	}
	if err != nil {
		return nil, err
	}
	conn := &dbusConn{
		c:        c,
		r:        bufio.NewReader(c),
		pending:  map[uint32]chan *dbusMessage{},
		handlers: map[string]dbusHandlerFunc{},
		closed:   make(chan struct{}),
	}
	if err := conn.auth(); err != nil {
		c.Close()
		return nil, err
	}
	// The read loop must run before Hello: its reply can only arrive
	// through the loop's dispatcher.
	go conn.readLoop()
	if _, err := conn.call("org.freedesktop.DBus", "/org/freedesktop/DBus",
		"org.freedesktop.DBus", "Hello", "", nil, 8*time.Second); err != nil {
		c.Close()
		return nil, err
	}
	return conn, nil
}

func (d *dbusConn) auth() error {
	// The leading NUL byte starts the protocol; EXTERNAL carries the uid.
	hexUID := strconv.FormatInt(int64(os.Getuid()), 16)
	d.wmu.Lock()
	_, err := d.c.Write([]byte("\x00AUTH EXTERNAL " + hexUID + "\r\n"))
	d.wmu.Unlock()
	if err != nil {
		return err
	}
	line, err := d.r.ReadString('\n')
	if err != nil {
		return err
	}
	line = strings.TrimRight(line, "\r\n")
	if !strings.HasPrefix(line, "OK ") {
		return fmt.Errorf("dbus: auth refused: %s", line)
	}
	d.wmu.Lock()
	_, err = d.c.Write([]byte("BEGIN\r\n"))
	d.wmu.Unlock()
	return err
}

func (d *dbusConn) close() {
	d.closeOnce.Do(func() {
		close(d.closed)
		d.c.Close()
	})
}

func (d *dbusConn) newSerial() uint32 {
	d.rmu.Lock()
	d.next++
	s := d.next
	d.rmu.Unlock()
	return s
}

// marshalFrame renders a full message; bodySig may be "".
func (d *dbusConn) marshalFrame(m *dbusMessage) ([]byte, error) {
	e := &dbusEncoder{}
	e.buf = append(e.buf, 'l', m.typ, m.flags, 1)
	bodyLen := uint32(0)
	if m.body != nil {
		bodyLen = uint32(len(m.body))
	}
	e.buf = binary.LittleEndian.AppendUint32(e.buf, bodyLen)
	e.buf = binary.LittleEndian.AppendUint32(e.buf, m.serial)

	fields := &dbusEncoder{}
	appendField := func(code byte, sig string, val any) {
		fields.align(8)
		fields.buf = append(fields.buf, code)
		_ = dbusAppendValue(fields, "v", dbusVariant{Sig: sig, Val: val})
	}
	if m.path != "" {
		appendField(1, "o", m.path)
	}
	if m.iface != "" {
		appendField(2, "s", m.iface)
	}
	if m.member != "" {
		appendField(3, "s", m.member)
	}
	if m.errName != "" {
		appendField(4, "s", m.errName)
	}
	if m.replyTo != 0 {
		appendField(5, "u", m.replyTo)
	}
	if m.destination != "" {
		appendField(6, "s", m.destination)
	}
	if m.sig != "" {
		appendField(8, "g", m.sig)
	}
	e.buf = binary.LittleEndian.AppendUint32(e.buf, uint32(len(fields.buf)))
	e.buf = append(e.buf, fields.buf...)
	e.align(8)
	out := append(e.buf, m.body...)
	return out, nil
}

// send writes a message on the wire.
func (d *dbusConn) send(m *dbusMessage) error {
	if m.serial == 0 {
		m.serial = d.newSerial()
	}
	frame, err := d.marshalFrame(m)
	if err != nil {
		return err
	}
	if os.Getenv("DBUS_DEBUG") != "" {
		log.Printf("dbus send: type=%d dest=%s member=%s frame=% x", m.typ, m.destination, m.member, frame)
	}
	d.wmu.Lock()
	defer d.wmu.Unlock()
	return writeAll(d.c, frame)
}

func writeAll(c net.Conn, b []byte) error {
	for len(b) > 0 {
		n, err := c.Write(b)
		if err != nil {
			return err
		}
		b = b[n:]
	}
	return nil
}

// call performs a method call and waits for its reply.
func (d *dbusConn) call(dest, path, iface, member, bodySig string, args []any, timeout time.Duration) (*dbusMessage, error) {
	body, err := d.renderBody(bodySig, args)
	if err != nil {
		return nil, err
	}
	m := &dbusMessage{
		typ:         dbusMsgMethodCall,
		destination: dest,
		path:        path,
		iface:       iface,
		member:      member,
		sig:         bodySig,
		body:        body,
	}
	m.serial = d.newSerial()
	ch := make(chan *dbusMessage, 1)
	d.pendingMu.Lock()
	d.pending[m.serial] = ch
	d.pendingMu.Unlock()
	defer func() {
		d.pendingMu.Lock()
		delete(d.pending, m.serial)
		d.pendingMu.Unlock()
	}()
	if err := d.send(m); err != nil {
		return nil, err
	}
	select {
	case reply := <-ch:
		if reply.typ == dbusMsgError {
			return nil, &dbusError{name: reply.errName, text: string(reply.body)}
		}
		return reply, nil
	case <-time.After(timeout):
		return nil, fmt.Errorf("dbus: %s.%s timed out", iface, member)
	case <-d.closed:
		return nil, errors.New("dbus: connection closed")
	}
}

func (d *dbusConn) renderBody(sig string, args []any) ([]byte, error) {
	if sig == "" {
		if len(args) > 0 {
			return nil, errors.New("dbus: args without signature")
		}
		return nil, nil
	}
	parts, _ := dbusSplitAll(sig)
	if len(parts) != len(args) {
		return nil, fmt.Errorf("dbus: signature %s wants %d args, got %d", sig, len(parts), len(args))
	}
	e := &dbusEncoder{}
	for i, p := range parts {
		if err := dbusAppendValue(e, p, args[i]); err != nil {
			return nil, err
		}
	}
	return e.buf, nil
}

// serve registers a handler for incoming method calls on interface.member.
func (d *dbusConn) serve(iface, member string, fn dbusHandlerFunc) {
	d.handlerMu.Lock()
	d.handlers[iface+"."+member] = fn
	d.handlerMu.Unlock()
}

// onSignal subscribes to a signal; rule is the full match string.
func (d *dbusConn) onSignal(rule string, fn func(*dbusMessage)) error {
	d.signalMu.Lock()
	d.signals = append(d.signals, fn)
	d.signalMu.Unlock()
	if rule != "" {
		if _, err := d.call("org.freedesktop.DBus", "/org/freedesktop/DBus",
			"org.freedesktop.DBus", "AddMatch", "s", []any{rule}, 8*time.Second); err != nil {
			return err
		}
	}
	return nil
}

func (d *dbusConn) reply(call *dbusMessage, sig string, args []any) {
	body, err := d.renderBody(sig, args)
	if err != nil {
		d.replyError(call, "org.freedesktop.DBus.Error.InvalidArgs", err.Error())
		return
	}
	_ = d.send(&dbusMessage{
		typ:         dbusMsgReturn,
		flags:       1, // no reply expected
		destination: call.sender,
		replyTo:     call.serial,
		sig:         sig,
		body:        body,
	})
}

func (d *dbusConn) replyError(call *dbusMessage, name, text string) {
	body, _ := d.renderBody("s", []any{text})
	_ = d.send(&dbusMessage{
		typ:         dbusMsgError,
		flags:       1,
		destination: call.sender,
		replyTo:     call.serial,
		errName:     name,
		sig:         "s",
		body:        body,
	})
}

// readFrame reads one message off the wire. Returns nil on clean shutdown.
func (d *dbusConn) readFrame() (*dbusMessage, error) {
	var head [16]byte
	if err := readFullBuf(d.r, head[:]); err != nil {
		return nil, err
	}
	if head[0] != 'l' {
		return nil, fmt.Errorf("dbus: only little-endian peers supported")
	}
	m := &dbusMessage{typ: head[1], flags: head[2]}
	bodyLen := binary.LittleEndian.Uint32(head[4:8])
	m.serial = binary.LittleEndian.Uint32(head[8:12])
	fieldsLen := binary.LittleEndian.Uint32(head[12:16])
	fields := make([]byte, fieldsLen)
	if err := readFullBuf(d.r, fields); err != nil {
		return nil, err
	}
	// header + fields padded to 8
	pad := (8 - (16+int(fieldsLen))%8) % 8
	if pad > 0 {
		if err := readFullBuf(d.r, make([]byte, pad)); err != nil {
			return nil, err
		}
	}
	if err := d.parseFields(m, fields); err != nil {
		return nil, err
	}
	if bodyLen > 0 {
		m.body = make([]byte, bodyLen)
		if err := readFullBuf(d.r, m.body); err != nil {
			return nil, err
		}
	}
	return m, nil
}

// readFullBuf fills b from r (the net.Conn readFull in window_linux.go
// works on raw connections; this one reads framed messages off the bufio
// wrapper).
func readFullBuf(r *bufio.Reader, b []byte) error {
	for len(b) > 0 {
		n, err := r.Read(b)
		if n > 0 {
			b = b[n:]
		}
		if err != nil {
			return err
		}
	}
	return nil
}

func (d *dbusConn) parseFields(m *dbusMessage, fields []byte) error {
	dec := &dbusDecoder{buf: fields}
	end := len(fields)
	for dec.off < end {
		if err := dec.align(8); err != nil {
			return err
		}
		if dec.off >= end {
			break
		}
		code := dec.buf[dec.off]
		dec.off++
		box, err := dbusReadValue(dec, "v")
		if err != nil {
			return err
		}
		v := box.(dbusVariant)
		switch code {
		case 1:
			m.path = v.Val.(string)
		case 2:
			m.iface = v.Val.(string)
		case 3:
			m.member = v.Val.(string)
		case 4:
			m.errName = v.Val.(string)
		case 5:
			m.replyTo = v.Val.(uint32)
		case 6:
			m.destination = v.Val.(string)
		case 7:
			m.sender = v.Val.(string)
		case 8:
			m.sig = v.Val.(string)
		}
	}
	return nil
}

func (d *dbusConn) readLoop() {
	for {
		m, err := d.readFrame()
		if err != nil {
			if os.Getenv("DBUS_DEBUG") != "" {
				log.Printf("dbus read: %v", err)
			}
			d.failPending(err)
			return
		}
		if os.Getenv("DBUS_DEBUG") != "" {
			log.Printf("dbus read: type=%d member=%s replyTo=%d err=%s", m.typ, m.member, m.replyTo, m.errName)
		}
		switch m.typ {
		case dbusMsgReturn, dbusMsgError:
			d.pendingMu.Lock()
			ch := d.pending[m.replyTo]
			delete(d.pending, m.replyTo)
			d.pendingMu.Unlock()
			if ch != nil {
				ch <- m
			}
		case dbusMsgMethodCall:
			d.dispatchCall(m)
		case dbusMsgSignal:
			d.signalMu.Lock()
			handlers := append([]func(*dbusMessage){}, d.signals...)
			d.signalMu.Unlock()
			for _, fn := range handlers {
				fn(m)
			}
		}
	}
}

func (d *dbusConn) dispatchCall(m *dbusMessage) {
	d.handlerMu.Lock()
	fn := d.handlers[m.iface+"."+m.member]
	d.handlerMu.Unlock()
	if fn == nil {
		if m.member == "Introspect" {
			d.reply(m, "s", []any{
				"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" " +
					"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n" +
					"<node>\n  <interface name=\"" + m.iface + "\"/>\n</node>\n"})
			return
		}
		d.replyError(m, "org.freedesktop.DBus.Error.UnknownMethod",
			m.iface+"."+m.member+" not implemented")
		return
	}
	replySig, replyArgs, derr := fn(m)
	if derr != nil {
		d.replyError(m, derr.name, derr.text)
		return
	}
	d.reply(m, replySig, replyArgs)
}

func (d *dbusConn) failPending(err error) {
	d.pendingMu.Lock()
	for serial, ch := range d.pending {
		ch <- &dbusMessage{typ: dbusMsgError, errName: "org.freedesktop.DBus.Error.Disconnected", body: []byte(err.Error())}
		delete(d.pending, serial)
	}
	d.pendingMu.Unlock()
}

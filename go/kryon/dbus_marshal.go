package kryon

// Minimal D-Bus wire codec: just the type set the desktop integration needs
// (strings, object paths, signatures, integers, booleans, byte arrays,
// string arrays, variant dictionaries, variants and nested structs), with
// the spec's alignment rules applied on both sides. Pure Go, no cgo.

import (
	"encoding/binary"
	"errors"
	"fmt"
	"math"
	"strings"
)

// dbusVariant is a typed value box: Sig is the D-Bus type string ("s", "u",
// "a{sv}", "(ia{sv}av)", …) and Val the Go value the encoders accept.
type dbusVariant struct {
	Sig string
	Val any
}

// dbusStruct carries a pre-rendered struct signature and its field values.
type dbusStruct struct {
	Sig string
	Val []any
}

type dbusEncoder struct {
	buf []byte
}

func (e *dbusEncoder) align(n int) {
	for pad := (n - len(e.buf)%n) % n; pad > 0; pad-- {
		e.buf = append(e.buf, 0)
	}
}

// padTo appends zero bytes until the buffer length is a multiple of n.
func (e *dbusEncoder) padTo(n int) {
	e.align(n)
}

func (e *dbusEncoder) u32(v uint32) {
	e.align(4)
	e.buf = binary.LittleEndian.AppendUint32(e.buf, v)
}

func (e *dbusEncoder) i32(v int32) { e.u32(uint32(v)) }

func (e *dbusEncoder) str(s string) {
	e.u32(uint32(len(s)))
	e.buf = append(e.buf, s...)
	e.buf = append(e.buf, 0)
}

func (e *dbusEncoder) sig(s string) {
	e.buf = append(e.buf, byte(len(s)))
	e.buf = append(e.buf, s...)
	e.buf = append(e.buf, 0)
}

// dbusAppendValue appends one complete type. sig must be exactly one
// complete type — callers split composite signatures themselves.
func dbusAppendValue(e *dbusEncoder, sig string, v any) error {
	if sig == "" {
		return errors.New("dbus: empty signature")
	}
	switch sig[0] {
	case 'y':
		b, ok := v.(byte)
		if !ok {
			return dbusTypeError(sig, v)
		}
		e.buf = append(e.buf, b)
	case 'b':
		b, ok := v.(bool)
		if !ok {
			return dbusTypeError(sig, v)
		}
		u := uint32(0)
		if b {
			u = 1
		}
		e.u32(u)
	case 'n':
		i, ok := v.(int16)
		if !ok {
			return dbusTypeError(sig, v)
		}
		e.align(2)
		e.buf = binary.LittleEndian.AppendUint16(e.buf, uint16(i))
	case 'q':
		u, ok := v.(uint16)
		if !ok {
			return dbusTypeError(sig, v)
		}
		e.align(2)
		e.buf = binary.LittleEndian.AppendUint16(e.buf, u)
	case 'i':
		switch n := v.(type) {
		case int32:
			e.i32(n)
		case int:
			e.i32(int32(n))
		default:
			return dbusTypeError(sig, v)
		}
	case 'u':
		switch n := v.(type) {
		case uint32:
			e.u32(n)
		case int:
			e.u32(uint32(n))
		default:
			return dbusTypeError(sig, v)
		}
	case 'x':
		n, ok := v.(int64)
		if !ok {
			return dbusTypeError(sig, v)
		}
		e.align(8)
		e.buf = binary.LittleEndian.AppendUint64(e.buf, uint64(n))
	case 't':
		n, ok := v.(uint64)
		if !ok {
			return dbusTypeError(sig, v)
		}
		e.align(8)
		e.buf = binary.LittleEndian.AppendUint64(e.buf, n)
	case 'd':
		n, ok := v.(float64)
		if !ok {
			return dbusTypeError(sig, v)
		}
		e.align(8)
		e.buf = binary.LittleEndian.AppendUint64(e.buf, math.Float64bits(n))
	case 's':
		s, ok := v.(string)
		if !ok {
			return dbusTypeError(sig, v)
		}
		e.str(s)
	case 'o':
		s, ok := v.(string)
		if !ok {
			return dbusTypeError(sig, v)
		}
		e.str(s)
	case 'g':
		s, ok := v.(string)
		if !ok {
			return dbusTypeError(sig, v)
		}
		e.sig(s)
	case 'v':
		box, ok := v.(dbusVariant)
		if !ok {
			return dbusTypeError(sig, v)
		}
		e.sig(box.Sig)
		if err := dbusAppendValue(e, box.Sig, box.Val); err != nil {
			return err
		}
	case 'a':
		elem := sig[1:]
		if strings.HasPrefix(elem, "{") {
			return dbusAppendDict(e, sig, v)
		}
		return dbusAppendArray(e, elem, v)
	case '(':
		return dbusAppendStruct(e, sig, v)
	default:
		return fmt.Errorf("dbus: unsupported type %q", sig)
	}
	return nil
}

func dbusAppendArray(e *dbusEncoder, elem string, v any) error {
	// Arrays marshal into a scratch buffer so the length prefix lands after
	// the element alignment padding, per spec.
	scratch := &dbusEncoder{}
	appendElem := func(item any) error {
		return dbusAppendValue(scratch, elem, item)
	}
	switch list := v.(type) {
	case []string:
		if elem != "s" {
			return dbusTypeError(elem, v)
		}
		for _, s := range list {
			if err := appendElem(s); err != nil {
				return err
			}
		}
	case []byte:
		if elem != "y" {
			return dbusTypeError(elem, v)
		}
		for _, b := range list {
			if err := appendElem(b); err != nil {
				return err
			}
		}
	case []uint32:
		if elem != "u" {
			return dbusTypeError(elem, v)
		}
		for _, u := range list {
			if err := appendElem(u); err != nil {
				return err
			}
		}
	case []int32:
		if elem != "i" {
			return dbusTypeError(elem, v)
		}
		for _, n := range list {
			if err := appendElem(n); err != nil {
				return err
			}
		}
	case []dbusVariant:
		if elem != "v" {
			return dbusTypeError(elem, v)
		}
		for _, box := range list {
			if err := appendElem(box); err != nil {
				return err
			}
		}
	case []dbusStruct:
		if !strings.HasPrefix(elem, "(") {
			return dbusTypeError(elem, v)
		}
		for _, st := range list {
			if err := appendElem(st); err != nil {
				return err
			}
		}
	case []any:
		for _, item := range list {
			if err := appendElem(item); err != nil {
				return err
			}
		}
	default:
		return dbusTypeError(elem, v)
	}
	e.u32(uint32(len(scratch.buf)))
	e.padTo(dbusAlignOf(elem))
	e.buf = append(e.buf, scratch.buf...)
	return nil
}

func dbusAppendDict(e *dbusEncoder, sig string, v any) error {
	// sig is "a{kvg}" style with a single complete dict entry type
	entry := strings.TrimSuffix(strings.TrimPrefix(sig, "a"), "")
	if !strings.HasPrefix(entry, "{") || !strings.HasSuffix(entry, "}") {
		return dbusTypeError(sig, v)
	}
	inner := entry[1 : len(entry)-1]
	keySig, valSig, ok := dbusSplitComplete(inner)
	if !ok {
		return dbusTypeError(sig, v)
	}
	dict, valid := v.(map[string]any)
	if !valid {
		return dbusTypeError(sig, v)
	}
	keys := make([]string, 0, len(dict))
	for k := range dict {
		keys = append(keys, k)
	}
	// deterministic order keeps tests stable
	for i := 0; i+1 < len(keys); i++ {
		for j := i + 1; j < len(keys); j++ {
			if keys[j] < keys[i] {
				keys[i], keys[j] = keys[j], keys[i]
			}
		}
	}
	scratch := &dbusEncoder{}
	for _, k := range keys {
		scratch.align(8) // dict entries align like structs
		if err := dbusAppendValue(scratch, keySig, k); err != nil {
			return err
		}
		if err := dbusAppendValue(scratch, valSig, dict[k]); err != nil {
			return err
		}
	}
	e.u32(uint32(len(scratch.buf)))
	e.padTo(8) // dict entries align like structs
	e.buf = append(e.buf, scratch.buf...)
	return nil
}

func dbusAppendStruct(e *dbusEncoder, sig string, v any) error {
	st, ok := v.(dbusStruct)
	if !ok || st.Sig != sig {
		return dbusTypeError(sig, v)
	}
	fields, rest := dbusSplitAll(sig[1 : len(sig)-1])
	if len(fields) != len(st.Val) {
		return fmt.Errorf("dbus: struct %s wants %d fields, got %d", sig, len(fields), len(st.Val))
	}
	e.align(8)
	for i, f := range fields {
		if err := dbusAppendValue(e, f, st.Val[i]); err != nil {
			return err
		}
	}
	_ = rest
	return nil
}

// dbusAlignOf returns the alignment of one complete type.
func dbusAlignOf(sig string) int {
	switch sig[0] {
	case 'y', 'g', 'v':
		return 1
	case 'n', 'q':
		return 2
	case 'b', 'i', 'u', 's', 'o', 'a':
		return 4
	default: // '(', 'x', 't', 'd', '{'
		return 8
	}
}

// dbusSplitComplete splits "keyvalue" at the first complete type boundary.
func dbusSplitComplete(sig string) (first, rest string, ok bool) {
	if sig == "" {
		return "", "", false
	}
	i := 0
	if sig[0] == 'a' {
		for i < len(sig) && sig[i] == 'a' {
			i++
		}
		if i >= len(sig) {
			return "", "", false
		}
		if sig[i] == '{' || sig[i] == '(' {
			depth, j := 0, i
			for ; j < len(sig); j++ {
				switch sig[j] {
				case '{', '(':
					depth++
				case '}', ')':
					depth--
					if depth == 0 {
						j++
					}
				}
				if depth == 0 {
					break
				}
			}
			if depth != 0 {
				return "", "", false
			}
			return sig[:j], sig[j:], true
		}
		return sig[:i+1], sig[i+1:], true
	}
	if sig[0] == '{' || sig[0] == '(' {
		depth, j := 0, 0
		for ; j < len(sig); j++ {
			switch sig[j] {
			case '{', '(':
				depth++
			case '}', ')':
				depth--
				if depth == 0 {
					j++
				}
			}
			if depth == 0 {
				break
			}
		}
		if depth != 0 {
			return "", "", false
		}
		return sig[:j], sig[j:], true
	}
	return sig[:1], sig[1:], true
}

// dbusSplitAll splits a signature into its complete types.
func dbusSplitAll(sig string) (parts []string, rest string) {
	for sig != "" {
		first, r, ok := dbusSplitComplete(sig)
		if !ok {
			return parts, sig
		}
		parts = append(parts, first)
		sig = r
	}
	return parts, ""
}

// ---- decoding ------------------------------------------------------------

type dbusDecoder struct {
	buf []byte
	off int
}

func (d *dbusDecoder) align(n int) error {
	for pad := (n - (d.off % n)) % n; pad > 0; pad-- {
		if d.off >= len(d.buf) || d.buf[d.off] != 0 {
			return errors.New("dbus: misaligned or missing padding")
		}
		d.off++
	}
	return nil
}

func (d *dbusDecoder) u32() (uint32, error) {
	if err := d.align(4); err != nil {
		return 0, err
	}
	if d.off+4 > len(d.buf) {
		return 0, errors.New("dbus: truncated uint32")
	}
	v := binary.LittleEndian.Uint32(d.buf[d.off:])
	d.off += 4
	return v, nil
}

// dbusReadValue decodes one complete type at the current offset.
func dbusReadValue(d *dbusDecoder, sig string) (any, error) {
	if sig == "" {
		return nil, errors.New("dbus: empty signature")
	}
	switch sig[0] {
	case 'y':
		if d.off >= len(d.buf) {
			return nil, errors.New("dbus: truncated byte")
		}
		b := d.buf[d.off]
		d.off++
		return b, nil
	case 'b':
		u, err := d.u32()
		if err != nil {
			return nil, err
		}
		return u != 0, nil
	case 'n':
		if err := d.align(2); err != nil {
			return nil, err
		}
		if d.off+2 > len(d.buf) {
			return nil, errors.New("dbus: truncated int16")
		}
		v := int16(binary.LittleEndian.Uint16(d.buf[d.off:]))
		d.off += 2
		return v, nil
	case 'q':
		if err := d.align(2); err != nil {
			return nil, err
		}
		if d.off+2 > len(d.buf) {
			return nil, errors.New("dbus: truncated uint16")
		}
		v := binary.LittleEndian.Uint16(d.buf[d.off:])
		d.off += 2
		return v, nil
	case 'i':
		u, err := d.u32()
		if err != nil {
			return nil, err
		}
		return int32(u), nil
	case 'u':
		return d.u32()
	case 'x':
		if err := d.align(8); err != nil {
			return nil, err
		}
		if d.off+8 > len(d.buf) {
			return nil, errors.New("dbus: truncated int64")
		}
		v := int64(binary.LittleEndian.Uint64(d.buf[d.off:]))
		d.off += 8
		return v, nil
	case 't':
		if err := d.align(8); err != nil {
			return nil, err
		}
		if d.off+8 > len(d.buf) {
			return nil, errors.New("dbus: truncated uint64")
		}
		v := binary.LittleEndian.Uint64(d.buf[d.off:])
		d.off += 8
		return v, nil
	case 'd':
		if err := d.align(8); err != nil {
			return nil, err
		}
		if d.off+8 > len(d.buf) {
			return nil, errors.New("dbus: truncated double")
		}
		v := math.Float64frombits(binary.LittleEndian.Uint64(d.buf[d.off:]))
		d.off += 8
		return v, nil
	case 's', 'o':
		n, err := d.u32()
		if err != nil {
			return nil, err
		}
		if uint32(d.off)+n+1 > uint32(len(d.buf)) {
			return nil, errors.New("dbus: truncated string")
		}
		s := string(d.buf[d.off : d.off+int(n)])
		d.off += int(n) + 1
		return s, nil
	case 'g':
		if d.off >= len(d.buf) {
			return nil, errors.New("dbus: truncated signature")
		}
		n := int(d.buf[d.off])
		d.off++
		if d.off+n+1 > len(d.buf) {
			return nil, errors.New("dbus: truncated signature body")
		}
		s := string(d.buf[d.off : d.off+n])
		d.off += n + 1
		return s, nil
	case 'v':
		inner, err := dbusReadValue(d, "g")
		if err != nil {
			return nil, err
		}
		val, err := dbusReadValue(d, inner.(string))
		if err != nil {
			return nil, err
		}
		return dbusVariant{Sig: inner.(string), Val: val}, nil
	case 'a':
		elem := sig[1:]
		if strings.HasPrefix(elem, "{") {
			return dbusReadDict(d, sig)
		}
		n, err := d.u32()
		if err != nil {
			return nil, err
		}
		align := dbusAlignOf(elem)
		if err := d.align(align); err != nil {
			return nil, err
		}
		end := d.off + int(n)
		if end > len(d.buf) {
			return nil, errors.New("dbus: truncated array")
		}
		switch elem {
		case "s", "o":
			var out []string
			for d.off < end {
				v, err := dbusReadValue(d, elem)
				if err != nil {
					return nil, err
				}
				out = append(out, v.(string))
			}
			d.off = end
			return out, nil
		case "y":
			out := append([]byte(nil), d.buf[d.off:end]...)
			d.off = end
			return out, nil
		case "u":
			var out []uint32
			for d.off < end {
				v, err := dbusReadValue(d, elem)
				if err != nil {
					return nil, err
				}
				out = append(out, v.(uint32))
			}
			d.off = end
			return out, nil
		case "i":
			var out []int32
			for d.off < end {
				v, err := dbusReadValue(d, elem)
				if err != nil {
					return nil, err
				}
				out = append(out, v.(int32))
			}
			d.off = end
			return out, nil
		case "v":
			var out []dbusVariant
			for d.off < end {
				v, err := dbusReadValue(d, elem)
				if err != nil {
					return nil, err
				}
				out = append(out, v.(dbusVariant))
			}
			d.off = end
			return out, nil
		default:
			var out []dbusStruct
			for d.off < end {
				v, err := dbusReadValue(d, elem)
				if err != nil {
					return nil, err
				}
				out = append(out, v.(dbusStruct))
			}
			d.off = end
			return out, nil
		}
	case '(':
		if err := d.align(8); err != nil {
			return nil, err
		}
		fields, _ := dbusSplitAll(sig[1 : len(sig)-1])
		vals := make([]any, 0, len(fields))
		for _, f := range fields {
			v, err := dbusReadValue(d, f)
			if err != nil {
				return nil, err
			}
			vals = append(vals, v)
		}
		return dbusStruct{Sig: sig, Val: vals}, nil
	default:
		return nil, fmt.Errorf("dbus: unsupported type %q", sig)
	}
}

func dbusReadDict(d *dbusDecoder, sig string) (any, error) {
	entry := sig[1:] // "{...}"
	inner := entry[1 : len(entry)-1]
	keySig, valSig, ok := dbusSplitComplete(inner)
	if !ok {
		return nil, dbusTypeError(sig, nil)
	}
	n, err := d.u32()
	if err != nil {
		return nil, err
	}
	if err := d.align(8); err != nil {
		return nil, err
	}
	end := d.off + int(n)
	if end > len(d.buf) {
		return nil, errors.New("dbus: truncated dict")
	}
	out := map[string]any{}
	for d.off < end {
		if err := d.align(8); err != nil {
			return nil, err
		}
		k, err := dbusReadValue(d, keySig)
		if err != nil {
			return nil, err
		}
		v, err := dbusReadValue(d, valSig)
		if err != nil {
			return nil, err
		}
		out[k.(string)] = v
	}
	d.off = end
	return out, nil
}

// dbusDecodeAll decodes a body buffer into Go values following sig.
func dbusDecodeAll(sig string, body []byte) ([]any, error) {
	parts, _ := dbusSplitAll(sig)
	d := &dbusDecoder{buf: body}
	var out []any
	for _, p := range parts {
		v, err := dbusReadValue(d, p)
		if err != nil {
			return nil, err
		}
		out = append(out, v)
	}
	return out, nil
}

func dbusTypeError(sig string, v any) error {
	return fmt.Errorf("dbus: value %T not usable for %q", v, sig)
}

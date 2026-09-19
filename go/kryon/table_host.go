package kryon

import (
	"math"
	"strings"
	"time"
)

func tableViewMetrics(props TableViewProps) TableViewMetrics {
	state := ButtonStateNormal
	if props.Disabled {
		state = ButtonStateDisabled
	}
	tableFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
		TableView_TableViewFactsFor(props.ClassName, int32(state)), int32(state))}
	headerFrame := tableViewRoleFrame(props.ClassName, state, TableView_TableViewHeaderRole())
	cellFrame := tableViewRoleFrame(props.ClassName, state, TableView_TableViewCellRole())
	dividerFrame := tableViewRoleFrame(props.ClassName, state, TableView_TableViewDividerRole())
	return TableView_TableViewMetricsFor(1, tableFrame, headerFrame, cellFrame, dividerFrame)
}

func tableViewRoleFrame(className int32, state ButtonState, role int32) StyleFrame {
	facts := TableView_TableViewRoleFactsFor(className, role, int32(state))
	value := ResolveActiveStyle(StyleData{}, facts, int32(state))
	return StyleFrame{Value: value}
}

func (r *runtime) TableView(props TableViewProps) int32 {
	props = normalizeTableViewProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	props.Disabled = props.Disabled || r.contentDisabled()
	if len(props.Columns) == 0 {
		return 0
	}

	metrics := tableViewMetrics(props)
	layout := TableView_TableViewLayoutFor(props.Bounds, int32(len(props.Rows)), props.RowHeight, props.HeaderHeight, props.FreezeRows, 1, metrics)
	rowH := layout.RowHeight
	headerH := layout.HeaderHeight
	body := layout.Body
	if props.ActivatedRow != nil {
		*props.ActivatedRow = -1
	}
	if props.ActivatedColumn != nil {
		*props.ActivatedColumn = -1
	}
	if props.RightClickedRow != nil {
		*props.RightClickedRow = -1
	}
	if props.RightClickedColumn != nil {
		*props.RightClickedColumn = -1
	}
	if props.PastedText != nil {
		*props.PastedText = ""
	}
	if props.PastedRow != nil {
		*props.PastedRow = -1
	}
	if props.PastedColumn != nil {
		*props.PastedColumn = -1
	}

	changed := int32(0)
	if r.tableResize.active && r.popupInputOwnerCaptures(r.tableResize.owner) {
		r.tableResize = tableResize{}
	}
	if r.tableResize.active && r.mouseReleased[MouseButtonLeft] &&
		(r.tableResize.id != props.ID || props.Disabled || !props.Resizable || len(props.ColumnWidths) == 0) {
		r.tableResize = tableResize{}
	}
	maxScroll := layout.MaxScroll
	if props.ScrollOffset != nil {
		*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
	}
	if props.Disabled {
		r.record(FrameOp{Kind: FrameOpTable, Bounds: props.Bounds, ID: props.ID, Disabled: true})
		r.drawTableOps(props, rowH, headerH)
		return 0
	}
	if props.Resizable && len(props.ColumnWidths) > 0 {
		for i := range r.clicks {
			click := &r.clicks[i]
			if click.consumed || click.button != MouseButtonLeft ||
				click.y < props.Bounds.Y || click.y >= props.Bounds.Y+float32(headerH) {
				continue
			}
			shift := tableHeaderShift(props, click.y)
			column, separatorX := tableSeparatorAtX(props, click.x-shift, float32(metrics.ResizeTolerance))
			separatorX += shift
			if column < 0 || int(column) >= len(props.ColumnWidths) {
				continue
			}
			click.consumed = true
			tolerance := float32(metrics.ResizeTolerance)
			r.consumeTap(Rectangle{X: separatorX - tolerance, Y: props.Bounds.Y, Width: tolerance * 2, Height: float32(headerH)})
			r.tableResize = tableResize{active: true, id: props.ID, column: column, startX: click.x, startWidth: tableColumnWidth(props, column), owner: r.currentPopupInputOwner()}
			break
		}
		if r.tableResize.active && r.tableResize.id == props.ID {
			if r.mouseDown[MouseButtonLeft] {
				minimum := TableView_TableViewMinimumColumnWidth(props.MinColumnWidth, 1, metrics)
				width := max32(minimum, r.tableResize.startWidth+int32(r.mousePos.X-r.tableResize.startX))
				if props.ColumnWidths[r.tableResize.column] != width {
					props.ColumnWidths[r.tableResize.column] = width
					changed = 1
				}
			}
			if r.mouseReleased[MouseButtonLeft] {
				r.tableResize = tableResize{}
			}
		}
	}
	if !props.Disabled && r.pointerCanReach(body) && props.ScrollOffset != nil && r.mouseWheel != 0 {
		*props.ScrollOffset = clamp32(*props.ScrollOffset-int32(r.mouseWheel)*rowH*3, 0, maxScroll)
		changed = 1
	}

	if props.ID != 0 {
		r.registerField(props.ID)
	}
	headerBounds := Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: props.Bounds.Width, Height: float32(headerH)}
	headerClickX, headerClicked := r.consumeMouseButtonPoint(MouseButtonLeft, headerBounds)
	if headerClicked && headerClickX >= props.Bounds.X && headerClickX < props.Bounds.X+props.Bounds.Width &&
		r.mousePos.Y >= props.Bounds.Y && r.mousePos.Y < props.Bounds.Y+float32(headerH) {
		col := tableColumnAtX(props, headerClickX-tableHeaderShift(props, r.mousePos.Y))
		previousSortColumn := int32(-1)
		if props.SortColumn != nil {
			previousSortColumn = *props.SortColumn
		}
		previousDirection := int32(0)
		if props.SortDirection != nil {
			previousDirection = *props.SortDirection
		}
		decision := TableView_TableViewSortDecisionFor(col, previousSortColumn, previousDirection)
		if decision.Changed {
			if props.SelectedRow != nil {
				*props.SelectedRow = decision.SelectedRow
				changed = 1
			}
			if props.SelectedColumn != nil {
				*props.SelectedColumn = decision.SelectedColumn
				changed = 1
			}
			if props.SortColumn != nil {
				*props.SortColumn = decision.SortColumn
				if props.SortDirection != nil {
					*props.SortDirection = decision.SortDirection
				}
				changed = 1
			}
		}
		if props.ID != 0 {
			r.setFocus(props.ID)
		}
	}

	if !props.CustomCells {
		if r.tableDrag.active && r.popupInputOwnerCaptures(r.tableDrag.owner) {
			r.tableDrag = tableDrag{}
		}
		for _, click := range r.consumeMouseButtonEvents(MouseButtonLeft, body) {
			row, col := tableCellAt(props, body, rowH, click.x, click.y)
			if row >= 0 && col >= 0 {
				changed |= setTableSelection(props, row, col, row, col)
				r.tableDrag = tableDrag{active: true, id: props.ID, startRow: row, startCol: col, owner: r.currentPopupInputOwner()}
				if props.ID != 0 {
					r.setFocus(props.ID)
				}
				if r.lastTableClick.id == props.ID && r.lastTableClick.row == row &&
					r.lastTableClick.column == col && click.when.Sub(r.lastTableClick.when) <= 450*time.Millisecond {
					if props.ActivatedRow != nil {
						*props.ActivatedRow = row
					}
					if props.ActivatedColumn != nil {
						*props.ActivatedColumn = col
					}
					changed = 1
				}
				r.lastTableClick = tableClick{id: props.ID, row: row, column: col, when: click.when}
			}
		}
		if r.tableDrag.active && r.tableDrag.id == props.ID && r.mouseDown[MouseButtonLeft] {
			row, col := tableCellAt(props, body, rowH, r.mousePos.X, r.mousePos.Y)
			if row >= 0 && col >= 0 {
				changed |= setTableSelection(props, r.tableDrag.startRow, r.tableDrag.startCol, row, col)
			}
		}
		if r.tableDrag.active && r.tableDrag.id == props.ID && r.mouseReleased[MouseButtonLeft] {
			r.tableDrag = tableDrag{}
		}

		if clickX, clicked := r.consumeMouseButtonPoint(MouseButtonRight, body); clicked {
			row, col := tableCellAt(props, body, rowH, clickX, r.mousePos.Y)
			if row >= 0 && col >= 0 {
				if props.RightClickedRow != nil {
					*props.RightClickedRow = row
				}
				if props.RightClickedColumn != nil {
					*props.RightClickedColumn = col
				}
				changed = 1
			}
		}

		if !r.contentDisabled() && !props.Disabled && props.ID != 0 && r.focusID == props.ID &&
			!r.popupFocusCaptures(props.ID) {
			changed |= r.handleTableKeys(props)
		}
	}

	r.record(FrameOp{Kind: FrameOpTable, Bounds: props.Bounds, ID: props.ID})
	r.drawTableOps(props, rowH, headerH)
	return changed
}

func normalizeTableViewProps(props TableViewProps) TableViewProps {
	if props.ColumnCount > 0 && int(props.ColumnCount) < len(props.Columns) {
		props.Columns = props.Columns[:props.ColumnCount]
	}
	if props.RowCount > 0 && int(props.RowCount) < len(props.Rows) {
		props.Rows = props.Rows[:props.RowCount]
	}
	if len(props.ColumnWidths) > len(props.Columns) {
		props.ColumnWidths = props.ColumnWidths[:len(props.Columns)]
	}
	if len(props.ColumnEnabled) > len(props.Columns) {
		props.ColumnEnabled = props.ColumnEnabled[:len(props.Columns)]
	}
	if len(props.ColumnOrder) > len(props.Columns) {
		props.ColumnOrder = props.ColumnOrder[:len(props.Columns)]
	}
	for i := range props.Rows {
		if props.Rows[i].CellCount > 0 && int(props.Rows[i].CellCount) < len(props.Rows[i].Cells) {
			props.Rows[i].Cells = props.Rows[i].Cells[:props.Rows[i].CellCount]
		}
	}
	return props
}

func (r *runtime) TableCellScope(props TableViewProps, row, col int32) Rectangle {
	props = normalizeTableViewProps(props)
	cell := TableCellRect(props, row, col)
	metrics := tableViewMetrics(props)
	layout := TableView_TableViewLayoutFor(props.Bounds, int32(len(props.Rows)), props.RowHeight, props.HeaderHeight, props.FreezeRows, 1, metrics)
	viewport := TableView_TableViewViewport(props.Bounds, layout, row >= layout.FrozenRows)
	left, right := max(cell.X, props.Bounds.X), min(cell.X+cell.Width, props.Bounds.X+props.Bounds.Width)
	top, bottom := max(viewport.Y, cell.Y), min(viewport.Y+viewport.Height, cell.Y+cell.Height)
	clip := Rectangle{X: left, Y: top, Width: max(float32(0), right-left), Height: max(float32(0), bottom-top)}
	r.DisabledScope(props.Disabled)
	r.ScrollScope(clip, int32(clip.Height), nil)
	return cell
}

func (r *runtime) TableCellEndScope() { r.ScrollEndScope(); r.DisabledEndScope() }

func TableCellRect(props TableViewProps, row, col int32) Rectangle {
	props = normalizeTableViewProps(props)
	if len(props.Columns) == 0 || row < 0 || col < 0 || int(row) >= len(props.Rows) || int(col) >= len(props.Columns) {
		return Rectangle{}
	}
	metrics := tableViewMetrics(props)
	layout := TableView_TableViewLayoutFor(props.Bounds, int32(len(props.Rows)), props.RowHeight, props.HeaderHeight, props.FreezeRows, 1, metrics)
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	x := props.Bounds.X
	found := false
	for _, logical := range tableDisplayColumns(props) {
		if logical == col {
			found = true
			break
		}
		x += float32(tableColumnWidth(props, logical))
	}
	if !found {
		return Rectangle{}
	}
	scrollLayout := TableView_TableViewScrollFor(scroll, layout.FrozenRows, layout.RowHeight, layout.ScrollBodyHeight)
	drawIndex := row
	if row >= layout.FrozenRows {
		drawIndex = layout.FrozenRows + row - scrollLayout.First
	}
	rowBounds := TableView_TableViewRowBounds(props.Bounds, layout, row, drawIndex, scrollLayout, row >= layout.FrozenRows)
	return TableView_TableViewCellBounds(rowBounds, int32(x), tableColumnWidth(props, col))
}

func (r *runtime) handleTableKeys(props TableViewProps) int32 {
	if props.SelectedRow == nil {
		return 0
	}
	changed := int32(0)
	handled := false
	selectionChanged := false
	selectedRow := *props.SelectedRow
	selectedCol := int32(-1)
	if props.SelectedColumn != nil {
		selectedCol = *props.SelectedColumn
	}
	row := int32(0)
	if selectedRow >= 0 {
		row = clamp32(selectedRow, 0, int32(len(props.Rows)-1))
	}
	displayColumns := tableDisplayColumns(props)
	if len(displayColumns) == 0 {
		return 0
	}
	colIndex := 0
	for i, candidate := range displayColumns {
		if candidate == selectedCol {
			colIndex = i
			break
		}
	}
	col := displayColumns[colIndex]
	for _, event := range r.inputEvents {
		if event.text != "" {
			continue
		}
		if event.shortcut {
			switch event.key {
			case KeyC, KeyX:
				if text, ok := tableClipboardText(props, selectedRow, selectedCol); ok {
					r.clipboard = text
					handled = true
					changed = 1
				}
			case KeyV:
				if props.PastedText != nil {
					*props.PastedText = r.clipboard
					if props.PastedRow != nil {
						*props.PastedRow = selectedRow
					}
					if props.PastedColumn != nil {
						*props.PastedColumn = selectedCol
					}
					handled = true
					changed = 1
				}
			}
			continue
		}
		switch event.key {
		case KeyUp:
			row = clamp32(row-1, 0, int32(len(props.Rows)-1))
			changed = 1
			selectionChanged = true
		case KeyDown:
			row = clamp32(row+1, 0, int32(len(props.Rows)-1))
			changed = 1
			selectionChanged = true
		case KeyLeft:
			if colIndex > 0 {
				colIndex--
			}
			col = displayColumns[colIndex]
			changed = 1
			selectionChanged = true
		case KeyRight:
			if colIndex < len(displayColumns)-1 {
				colIndex++
			}
			col = displayColumns[colIndex]
			changed = 1
			selectionChanged = true
		case KeyTab:
			if event.shift {
				if colIndex > 0 {
					colIndex--
				} else {
					colIndex = len(displayColumns) - 1
					row = clamp32(row-1, 0, int32(len(props.Rows)-1))
				}
			} else if colIndex < len(displayColumns)-1 {
				colIndex++
			} else {
				colIndex = 0
				row = clamp32(row+1, 0, int32(len(props.Rows)-1))
			}
			col = displayColumns[colIndex]
			changed = 1
			selectionChanged = true
		case KeyEnter, KeyF2:
			if props.ActivatedRow != nil {
				*props.ActivatedRow = row
			}
			if props.ActivatedColumn != nil {
				*props.ActivatedColumn = col
			}
			changed = 1
			selectionChanged = true
		case KeyEscape:
			if *props.SelectedRow >= 0 || props.SelectedColumn != nil && *props.SelectedColumn >= 0 {
				row = -1
				col = -1
				changed = 1
				selectionChanged = true
			}
		}
	}
	if selectionChanged {
		*props.SelectedRow = row
		if props.SelectedColumn != nil {
			*props.SelectedColumn = col
		}
		if row >= 0 {
			r.scrollTableSelectionIntoView(props)
		}
	}
	if handled || changed != 0 {
		r.inputEvents = nil
	}
	return changed
}

func tableClipboardText(props TableViewProps, row, col int32) (string, bool) {
	if props.CopyText != nil {
		return *props.CopyText, true
	}
	switch {
	case row >= 0 && int(row) < len(props.Rows) && col >= 0:
		return tableCellText(props, row, col), true
	case row >= 0 && int(row) < len(props.Rows):
		cells := make([]string, len(props.Columns))
		for c := range cells {
			cells[c] = tableCellText(props, row, int32(c))
		}
		return strings.Join(cells, "\t"), true
	case col >= 0 && int(col) < len(props.Columns):
		cells := make([]string, len(props.Rows))
		for r := range cells {
			cells[r] = tableCellText(props, int32(r), col)
		}
		return strings.Join(cells, "\n"), true
	}
	return "", false
}

func setTableSelection(props TableViewProps, startRow, startCol, endRow, endCol int32) int32 {
	changed := int32(0)
	if props.SelectedRow != nil && *props.SelectedRow != endRow {
		*props.SelectedRow = endRow
		changed = 1
	}
	if props.SelectedColumn != nil && *props.SelectedColumn != endCol {
		*props.SelectedColumn = endCol
		changed = 1
	}
	if props.SelectionStartRow != nil && *props.SelectionStartRow != startRow {
		*props.SelectionStartRow = startRow
		changed = 1
	}
	if props.SelectionStartColumn != nil && *props.SelectionStartColumn != startCol {
		*props.SelectionStartColumn = startCol
		changed = 1
	}
	if props.SelectionEndRow != nil && *props.SelectionEndRow != endRow {
		*props.SelectionEndRow = endRow
		changed = 1
	}
	if props.SelectionEndColumn != nil && *props.SelectionEndColumn != endCol {
		*props.SelectionEndColumn = endCol
		changed = 1
	}
	return changed
}

func tableCellText(props TableViewProps, row, col int32) string {
	if row < 0 || int(row) >= len(props.Rows) || col < 0 {
		return ""
	}
	cells := props.Rows[row].Cells
	if int(col) >= len(cells) {
		return ""
	}
	return cells[col]
}

func tableSelectionRange(props TableViewProps) (int32, int32, int32, int32, bool) {
	if props.SelectionStartRow == nil || props.SelectionStartColumn == nil ||
		props.SelectionEndRow == nil || props.SelectionEndColumn == nil {
		return 0, 0, 0, 0, false
	}
	startRow, startCol := *props.SelectionStartRow, *props.SelectionStartColumn
	endRow, endCol := *props.SelectionEndRow, *props.SelectionEndColumn
	if startRow < 0 || startCol < 0 || endRow < 0 || endCol < 0 {
		return 0, 0, 0, 0, false
	}
	if startRow > endRow {
		startRow, endRow = endRow, startRow
	}
	if startCol > endCol {
		startCol, endCol = endCol, startCol
	}
	return startRow, startCol, endRow, endCol, true
}

func tableCellSelected(props TableViewProps, row, col, selectedRow, selectedCol int32) bool {
	if startRow, startCol, endRow, endCol, ok := tableSelectionRange(props); ok {
		return row >= startRow && row <= endRow && col >= startCol && col <= endCol
	}
	return row == selectedRow && col == selectedCol || selectedRow < 0 && col == selectedCol
}

func (r *runtime) scrollTableSelectionIntoView(props TableViewProps) {
	if props.ScrollOffset == nil || props.SelectedRow == nil {
		return
	}
	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 28
	}
	bodyH := int32(props.Bounds.Height) - max32(30, props.HeaderHeight)
	if bodyH <= 0 {
		return
	}
	frozenRows := tableFrozenRows(props, rowH, bodyH)
	if *props.SelectedRow < frozenRows {
		return
	}
	viewH := bodyH - frozenRows*rowH
	if viewH <= 0 {
		return
	}
	top := (*props.SelectedRow - frozenRows) * rowH
	bottom := top + rowH
	if top < *props.ScrollOffset {
		*props.ScrollOffset = top
	} else if bottom > *props.ScrollOffset+viewH {
		*props.ScrollOffset = bottom - viewH
	}
	maxScroll := max32(0, (int32(len(props.Rows))-frozenRows)*rowH-viewH)
	*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
}

func tableHeaderShift(props TableViewProps, y float32) float32 {
	a := float64(props.HeaderAngle)
	if a == 0 || math.IsNaN(a) || math.IsInf(a, 0) {
		return 0
	}
	a = math.Max(-89, math.Min(89, a))
	h := float32(max32(30, props.HeaderHeight))
	return -(h - (y - props.Bounds.Y)) / float32(math.Tan(a*math.Pi/180))
}

func (r *runtime) drawTableOps(props TableViewProps, rowH, headerH int32) {
	disabledColor := func(color Color) Color {
		if props.Disabled {
			return r.Fade(color, 0.45)
		}
		return color
	}
	tableState := ButtonStateNormal
	if props.Disabled {
		tableState = ButtonStateDisabled
	}
	surfaceFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, tableState, props.Disabled, false,
		props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewPanelRole())
	cellStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, tableState, props.Disabled, false,
		props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewCellRole()).Value)
	rowFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, tableState, props.Disabled, false,
		props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewRowRole())
	rowStyle := unpackStyle(rowFrame.Value)
	selectedFrame := simpleStyleFrameWithClassRole(ButtonToneAccent, ButtonStateSelected, props.Disabled, true,
		props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewSelectionRole())
	selectedStyle := unpackStyle(selectedFrame.Value)
	dividerFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, tableState, props.Disabled, false,
		props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewDividerRole())
	dividerStyle := unpackStyle(dividerFrame.Value)
	metrics := tableViewMetrics(props)
	tableOp := styleFrameRectOp(props.Bounds, Rectangle{}, surfaceFrame)
	tableOp.Color = disabledColor(tableOp.Color)
	tableOp.Disabled = props.Disabled
	r.record(tableOp)
	displayColumns := tableDisplayColumns(props)
	if len(displayColumns) == 0 {
		return
	}
	selectedRow := int32(-1)
	selectedCol := int32(-1)
	if props.SelectedRow != nil {
		selectedRow = *props.SelectedRow
	}
	if props.SelectedColumn != nil {
		selectedCol = *props.SelectedColumn
	}
	fallbackFont := Text12
	if rowH >= 28 {
		fallbackFont = Text14
	}
	cellFont, cellFontID := styleTextFace(cellStyle, fallbackFont)
	layout := TableView_TableViewLayoutFor(props.Bounds, int32(len(props.Rows)), rowH, headerH, props.FreezeRows, 1, metrics)
	for _, col := range displayColumns {
		c := int(col)
		x := props.Bounds.X
		for _, logical := range displayColumns {
			if logical == col {
				break
			}
			x += float32(tableColumnWidth(props, logical))
		}
		rect := TableView_TableViewHeaderBounds(props.Bounds, int32(x), tableColumnWidth(props, col), headerH)
		selected := false
		if selectedRow < 0 && selectedCol == col {
			selected = true
		}
		headerFrame := simpleStyleFrameWithClassRole(func() ButtonTone {
			if selected {
				return ButtonToneAccent
			}
			return ButtonToneNeutral
		}(), func() ButtonState {
			if props.Disabled {
				return ButtonStateDisabled
			}
			if selected {
				return ButtonStateSelected
			}
			return ButtonStateNormal
		}(), props.Disabled, selected, props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewHeaderRole())
		headerStyle := unpackStyle(headerFrame.Value)
		headerFont, headerFontID := styleTextFace(headerStyle, fallbackFont)
		shift := tableHeaderShift(props, rect.Y)
		var polygon [4]Vector2
		if shift != 0 {
			polygon = [4]Vector2{{rect.X + shift, rect.Y}, {rect.X + rect.Width + shift, rect.Y}, {rect.X + rect.Width, rect.Y + rect.Height}, {rect.X, rect.Y + rect.Height}}
		}
		headerClip := r.scrollClip(Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: props.Bounds.Width, Height: float32(headerH)})
		headerOp := styleFrameRectOp(rect, props.Bounds, headerFrame)
		headerOp.Polygon = polygon
		headerOp.HasPolygon = shift != 0
		headerOp.Color = disabledColor(headerOp.Color)
		headerOp.Row = -1
		headerOp.Column = col
		headerOp.Selected = selected
		headerOp.Disabled = props.Disabled
		r.record(headerOp)
		if shift != 0 {
			r.ops[len(r.ops)-1].Clip = headerClip
			r.ops[len(r.ops)-1].HasClip = true
		}
		textOp := FrameOp{Kind: FrameOpText, Bounds: tableTextBounds(rect, metrics.HeaderTextPadX), Text: elideTextWithFont(props.Columns[c], rect.Width-float32(metrics.HeaderTextPadX*2), headerFont, headerFontID), Color: disabledColor(headerStyle.Foreground), Opacity: headerStyle.Opacity, FontSize: headerFont, FontID: headerFontID, Row: -1, Column: col, Disabled: props.Disabled}
		angle := props.HeaderAngle
		if math.IsNaN(float64(angle)) || math.IsInf(float64(angle), 0) {
			angle = 0
		}
		angle = max(float32(-89), min(float32(89), angle))
		if angle != 0 {
			textOp.Polygon = polygon
			textOp.HasPolygon = true
			textOp.Text, textOp.Rotation = props.Columns[c], angle
			textOp.Bounds.X, textOp.Bounds.Y = rect.X+float32(metrics.HeaderTextPadX), rect.Y+float32(metrics.HeaderTextPadX)
			if angle > 0 {
				textOp.Bounds.X += shift
			}
			if angle < 0 {
				textOp.Bounds.Y = rect.Y + rect.Height - float32(metrics.HeaderTextPadX)
			}
		}
		r.record(textOp)
		if angle != 0 {
			r.ops[len(r.ops)-1].Clip = headerClip
			r.ops[len(r.ops)-1].HasClip = true
		}
		if props.Resizable && c < len(props.ColumnWidths) {
			r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: rect.X + rect.Width + shift - 1, Y: rect.Y, Width: -shift, Height: rect.Height}, Color: disabledColor(dividerStyle.Border), Column: col, Disabled: props.Disabled})
			r.ops[len(r.ops)-1].Clip, r.ops[len(r.ops)-1].HasClip = headerClip, true
		}
	}
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	frozenRows := layout.FrozenRows
	scrollLayout := TableView_TableViewScrollFor(scroll, frozenRows, rowH, layout.ScrollBodyHeight)
	first := scrollLayout.First
	visible := scrollLayout.VisibleRows
	drawRow := func(row int32) {
		clip := TableView_TableViewViewport(props.Bounds, layout, row >= frozenRows)
		start := len(r.ops)
		defer func() {
			for i := start; i < len(r.ops); i++ {
				r.ops[i].Clip = r.scrollClip(clip)
				r.ops[i].HasClip = true
			}
		}()
		rowY := TableCellRect(props, row, displayColumns[0]).Y
		rowRect := Rectangle{X: props.Bounds.X, Y: rowY, Width: props.Bounds.Width, Height: float32(rowH)}
		if row%2 == 1 {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: rowRect, Color: disabledColor(rowStyle.Background), BorderColor: disabledColor(rowStyle.Border), Radius: rowStyle.Radius, BorderWidth: rowStyle.BorderWidth, Material: rowStyle.Material, Opacity: rowStyle.Opacity, Row: row, Disabled: props.Disabled})
		}
		if row == selectedRow && selectedCol < 0 {
			op := styleFrameRectOp(rowRect, props.Bounds, selectedFrame)
			op.Color = disabledColor(op.Color)
			op.Row = row
			op.Column = -1
			op.Selected = true
			op.Disabled = props.Disabled
			r.record(op)
		}
		for _, col := range displayColumns {
			c := int(col)
			rect := TableCellRect(props, row, col)
			if rect.Y+rect.Height < props.Bounds.Y+float32(headerH) || rect.Y > props.Bounds.Y+props.Bounds.Height {
				continue
			}
			cellTextColor := cellStyle.Foreground
			selectedCell := tableCellSelected(props, row, col, selectedRow, selectedCol)
			if selectedCell {
				op := styleFrameRectOp(rect, props.Bounds, selectedFrame)
				op.Color = disabledColor(op.Color)
				op.Row = row
				op.Column = col
				op.Selected = true
				op.Disabled = props.Disabled
				op.SelectionStartRow = valueOr32(props.SelectionStartRow, -1)
				op.SelectionStartCol = valueOr32(props.SelectionStartColumn, -1)
				op.SelectionEndRow = valueOr32(props.SelectionEndRow, -1)
				op.SelectionEndCol = valueOr32(props.SelectionEndColumn, -1)
				r.record(op)
				cellTextColor = selectedStyle.Foreground
			}
			text := ""
			if int(row) < len(props.Rows) && c < len(props.Rows[row].Cells) {
				text = props.Rows[row].Cells[c]
			}
			textOpacity := cellStyle.Opacity
			textFont := cellFont
			textFontID := cellFontID
			if selectedCell {
				textOpacity = selectedStyle.Opacity
				textFont, textFontID = styleTextFaceWithFallback(selectedStyle, cellFont, cellFontID)
			}
			r.record(FrameOp{Kind: FrameOpText, Bounds: tableTextBounds(rect, metrics.HeaderTextPadX), Text: elideTextWithFont(text, rect.Width-float32(metrics.HeaderTextPadX*2), textFont, textFontID), Color: disabledColor(cellTextColor), Opacity: textOpacity, FontSize: textFont, FontID: textFontID, Row: row, Column: col, Disabled: props.Disabled})
		}
	}
	for row := int32(0); row < frozenRows; row++ {
		drawRow(row)
	}
	for i := int32(0); i < visible && first+i < int32(len(props.Rows)); i++ {
		drawRow(first + i)
	}
}

func tableTextBounds(rect Rectangle, inset int32) Rectangle {
	pad := float32(inset)
	return Rectangle{X: rect.X + pad, Y: rect.Y + pad, Width: rect.Width - pad*2, Height: rect.Height - pad - 2}
}

func tableCellAt(props TableViewProps, body Rectangle, rowH int32, x, y float32) (int32, int32) {
	props = normalizeTableViewProps(props)
	if rowH <= 0 || len(props.Columns) == 0 {
		return -1, -1
	}
	metrics := tableViewMetrics(props)
	layout := TableView_TableViewLayoutFor(props.Bounds, int32(len(props.Rows)), rowH, props.HeaderHeight, props.FreezeRows, 1, metrics)
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	frozenRows := layout.FrozenRows
	frozenHeight := float32(frozenRows * layout.RowHeight)
	localY := y - body.Y
	row := int32(0)
	if localY < frozenHeight {
		row = int32(localY / float32(layout.RowHeight))
	} else {
		row = frozenRows + int32((localY-frozenHeight+float32(scroll))/float32(layout.RowHeight))
	}
	if row < 0 || int(row) >= len(props.Rows) {
		return -1, -1
	}
	col := tableColumnAtX(props, x)
	if col < 0 {
		return -1, -1
	}
	return row, col
}

func tableColumnAtX(props TableViewProps, x float32) int32 {
	props = normalizeTableViewProps(props)
	if len(props.Columns) == 0 {
		return -1
	}
	cursor := props.Bounds.X
	for _, col := range tableDisplayColumns(props) {
		w := float32(tableColumnWidth(props, col))
		if x >= cursor && x < cursor+w {
			return col
		}
		cursor += w
	}
	return -1
}

func tableColumnWidth(props TableViewProps, col int32) int32 {
	props = normalizeTableViewProps(props)
	if col < 0 || int(col) >= len(props.Columns) {
		return 0
	}
	if int(col) < len(props.ColumnWidths) && props.ColumnWidths[col] > 0 {
		return props.ColumnWidths[col]
	}
	visible := len(tableDisplayColumns(props))
	if visible == 0 {
		return 0
	}
	return TableView_TableViewDefaultColumnWidth(int32(props.Bounds.Width), int32(visible))
}

func tableFrozenRows(props TableViewProps, rowH, bodyHeight int32) int32 {
	return TableView_TableViewFrozenRows(props.FreezeRows, int32(len(props.Rows)), bodyHeight, rowH)
}

func tableSeparatorAtX(props TableViewProps, x, tolerance float32) (int32, float32) {
	cursor := props.Bounds.X
	for _, column := range tableDisplayColumns(props) {
		cursor += float32(tableColumnWidth(props, column))
		if x >= cursor-tolerance && x <= cursor+tolerance {
			return column, cursor
		}
	}
	return -1, 0
}

func tableDisplayColumns(props TableViewProps) []int32 {
	count := len(props.Columns)
	if count == 0 {
		return nil
	}
	enabled := func(col int) bool {
		return col >= len(props.ColumnEnabled) || props.ColumnEnabled[col] != 0
	}
	columns := make([]int32, 0, count)
	seen := make([]bool, count)
	for _, requested := range props.ColumnOrder {
		col := int(requested)
		if col >= 0 && col < count && !seen[col] {
			seen[col] = true
			if enabled(col) {
				columns = append(columns, requested)
			}
		}
	}
	for col := 0; col < count; col++ {
		if !seen[col] && enabled(col) {
			columns = append(columns, int32(col))
		}
	}
	return columns
}

package kryon

func BeginFlexCursor(props FlexProps, count int32, totalItemExtent float32) FlexCursor {
	return Layout_BeginFlexCursor(props, count, totalItemExtent)
}

func FlexStep(cursor FlexCursor, width, height float32) FlexCursor {
	return Layout_FlexStep(cursor, width, height)
}

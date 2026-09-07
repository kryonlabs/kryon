package kryon

// BeginTabBar renders the canonical tab header and opens a scope in which
// BeginTabItem exposes only the selected tab's arbitrary child widgets.
func (r *runtime) BeginTabBar(props TabBarProps, selectedIndex *int32) bool {
	count := props.Count
	if count <= 0 || count > int32(len(props.Tabs)) {
		count = int32(len(props.Tabs))
	}
	if count <= 0 || props.Bounds.Width <= 0 || props.Bounds.Height <= 0 {
		return false
	}

	selected := props.SelectedIndex
	if selectedIndex != nil {
		selected = *selectedIndex
	}
	if selected < 0 || selected >= count {
		selected = 0
	}
	props.Count = count
	props.SelectedIndex = selected
	if clicked := r.TabBar(props); clicked >= 0 {
		selected = clicked
	}
	if selectedIndex != nil {
		*selectedIndex = selected
	}
	r.tabBarScopes = append(r.tabBarScopes, tabBarScope{
		count:    count,
		selected: selected,
	})
	return true
}

func (r *runtime) BeginTabItem(index int32) bool {
	if len(r.tabBarScopes) == 0 {
		panic("BeginTabItem without BeginTabBar")
	}
	scope := &r.tabBarScopes[len(r.tabBarScopes)-1]
	if scope.itemOpen {
		panic("BeginTabItem before EndTabItem")
	}
	if index < 0 || index >= scope.count || index != scope.selected {
		return false
	}
	scope.itemOpen = true
	return true
}

func (r *runtime) EndTabItem() {
	if len(r.tabBarScopes) == 0 ||
		!r.tabBarScopes[len(r.tabBarScopes)-1].itemOpen {
		panic("EndTabItem without selected BeginTabItem")
	}
	r.tabBarScopes[len(r.tabBarScopes)-1].itemOpen = false
}

func (r *runtime) EndTabBar() {
	if len(r.tabBarScopes) == 0 {
		panic("EndTabBar without BeginTabBar")
	}
	n := len(r.tabBarScopes)
	if r.tabBarScopes[n-1].itemOpen {
		panic("EndTabBar before EndTabItem")
	}
	r.tabBarScopes[n-1] = tabBarScope{}
	r.tabBarScopes = r.tabBarScopes[:n-1]
}

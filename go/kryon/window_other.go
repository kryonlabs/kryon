//go:build !linux

package kryon

import "errors"

func openWindowRuntime(AppConfig) (Runtime, error) {
	return nil, errors.New("native Go window runtime is only implemented on linux")
}

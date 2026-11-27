let clicks: s32 = s32(0);

export function RecordGalleryClick(clicked: boolean): void {
    if (clicked) {
        clicks += s32(1);
    }
}

export function GalleryClickCount(): s32 {
    return clicks;
}

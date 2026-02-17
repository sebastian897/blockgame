void test() {
    Canvas c1 = (Canvas){{0},
                 (PixelSize){400, 200},
                 NULL};
    Canvas c2 = (Canvas){{0},
                 (PixelSize){400, 200},
                 NULL};
    Canvas c3 = (Canvas){{0},
                 (PixelSize){200, 400},
                 NULL};
    Canvas c4 = (Canvas){{0},
                 (PixelSize){200, 400},
                 NULL};
    Canvas c5 = (Canvas){{0},
                 (PixelSize){400, 200},
                 NULL};
    Canvas c6 = (Canvas){{0},
                 (PixelSize){200, 400},
                 NULL};
    Canvas c7 = (Canvas){{0},
                 (PixelSize){400, 200},
                 NULL};
    
    StackLeft(StackAbove(StackAbove(&c1, &c2), StackLeft(&c6, &c7)), StackAbove(&c5, StackLeft(&c3, &c4)));
; 
    // StackAbove(&c3, PrependCanvas(&c3, &c4));
    // StackLeft(&c3, PrependCanvas(&c1, &c3));
    Rectangle rec1 = {c1.origin.x,c1.origin.y, c1.size.width,c1.size.height};
    DrawRectangleRec(rec1, palette[2]);
    Rectangle rec2 = {c2.origin.x,c2.origin.y, c2.size.width,c2.size.height};
    DrawRectangleRec(rec2, palette[3]);
    Rectangle rec3 = {c3.origin.x,c3.origin.y, c3.size.width,c3.size.height};
    DrawRectangleRec(rec3, palette[4]);
    Rectangle rec4 = {c4.origin.x,c4.origin.y, c4.size.width,c4.size.height};
    DrawRectangleRec(rec4, palette[5]);
    Rectangle rec5 = {c5.origin.x,c5.origin.y, c5.size.width, c5.size.height};
    DrawRectangleRec(rec5, palette[6]);
    Rectangle rec6 = {c6.origin.x,c6.origin.y, c6.size.width, c6.size.height};
    DrawRectangleRec(rec6, palette[7]);
    Rectangle rec7 = {c7.origin.x,c7.origin.y, c7.size.width, c7.size.height};
    DrawRectangleRec(rec7, palette[8]);
}

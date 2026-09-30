// ime_shim.m —— P-M3 IME macOS 臂(§6.4 composer 一等状态;尖刺 3c9662f)
// runtime-swizzle GLFWContentView 三面:setMarkedText:selectedRange:replacementRange:
// (组词镜像)/firstRectForCharacterRange:actualRange:(候选窗跟随光标);
// insertText 提交链不经此(GetCharPressed 原生已通,尖刺静态闭合)。
// 暴露 C API:gui_ime_preedit/gui_ime_caret/gui_ime_set_caret/gui_ime_swizzle。
// 编译:cc -x objective-c(Darwin only;非 Darwin 由 ctron_gui.c 的 __APPLE__
// 门控不调用不链接)。

#import <objc/runtime.h>
#import <objc/message.h>
#import <AppKit/AppKit.h>
#import <string.h>
#import <stdio.h>
#import <stdlib.h>

// C 全局驻 ctron_gui.c(真臂/回退/test 注入三方同源;此处 extern)
extern char g_ime_pre[256];
extern int g_ime_has_pre;
extern int g_ime_crect[4];
extern int g_ime_cx;
extern int gui_ime_prev_set(const char* s);
extern int gui_ime_prev_eq(const char* s);
static int g_ime_swizzled = 0;

static int g_ime_trace(void) {
    const char* t = getenv("CTRON_GUI_IME_TRACE");
    return t != NULL && t[0] != 0;
}

static void ime_mirror(const char* s) {
    int n = (int)strlen(s);
    if (n > 255) { n = 255; }
    memcpy(g_ime_pre, s, (size_t)n);
    g_ime_pre[n] = 0;
    g_ime_has_pre = n > 0;
    if (n == 0) { gui_ime_prev_set(""); } // 会话结束清快照
    if (g_ime_trace()) {
        fprintf(stderr, "[IME] preedit=\"%s\" len=%d\n", g_ime_pre, n);
    }
}

// ---- swizzle 目标 impl(交换后:原 selector 进这里;调 swz 版 = 原实现)----

static void ime_swz_setMarkedText(id self, SEL _cmd, id str, NSRange sel, NSRange rep) {
    // 调原实现(交换后原实现挂在 swz 名下)
    ((void (*)(id, SEL, id, NSRange, NSRange))objc_msgSend)(self,
        sel_registerName("ime_swz_setMarkedText:selectedRange:replacementRange:"),
        str, sel, rep);
    // 镜像组词串(GLFWContentView.markedText ivar)
    Ivar iv = class_getInstanceVariable(object_getClass(self), "markedText");
    if (iv != NULL) {
        id m = object_getIvar(self, iv);
        if (m != NULL) {
            NSString* s = nil;
            if ([m isKindOfClass:[NSAttributedString class]]) {
                s = [(NSAttributedString*)m string];
            } else if ([m isKindOfClass:[NSString class]]) {
                s = (NSString*)m;
            }
            if (s != nil) {
                ime_mirror([s UTF8String]);
                return;
            }
        }
    }
    ime_mirror("");
}

// commit/解组 = 组词结束:镜像清空(GLFW 的 insertText 不清自身 markedText,
// 第三方 IME(搜狗)也不调 setMarkedText:"" ——不清则内联渲染残留过期组词串)
static void ime_swz_insertText(id self, SEL _cmd, id str, NSRange rep) {
    ((void (*)(id, SEL, id, NSRange))objc_msgSend)(self,
        sel_registerName("ime_swz_insertText:replacementRange:"), str, rep);
    ime_mirror("");
}

static void ime_swz_unmarkText(id self, SEL _cmd) {
    ((void (*)(id, SEL))objc_msgSend)(self, sel_registerName("ime_swz_unmarkText"));
    ime_mirror("");
}

static NSRect ime_swz_firstRect(id self, SEL _cmd, NSRange range, NSRangePointer actual) {
    // 域包反喂了光标 rect → 换算屏幕坐标返回(候选窗跟随光标);
    // 未反喂(无聚焦 input)→ 走原实现(旧行为:视图原点)
    if (g_ime_trace()) {
        fprintf(stderr, "[IME] firstRect ASKED crect=%d,%d %dx%d\n",
                g_ime_crect[0], g_ime_crect[1], g_ime_crect[2], g_ime_crect[3]);
    }
    if (g_ime_crect[2] > 0 && g_ime_crect[3] > 0) {
        NSView* v = (NSView*)self;
        NSWindow* w = [v window];
        if (w != NULL) {
            // 确定性换算(勿用 convertRect 链——flipped 视图歧义致面板漂移,用户
            // 实测三态:左下/框下/左上):fed 坐标 = raylib 窗口系(内容视图左上原
            // 点,y 向下);屏幕系 = AppKit 左下原点。contentView frame 给标题栏偏移。
            NSRect wf = [w frame];
            NSRect cb = [[w contentView] frame];
            CGFloat ax = (CGFloat)g_ime_crect[0];
            if (g_ime_cx >= 0) { ax += (CGFloat)g_ime_cx; }
            CGFloat sx = wf.origin.x + cb.origin.x + ax;
            CGFloat sy = wf.origin.y + cb.origin.y + cb.size.height
                       - (CGFloat)g_ime_crect[1] - (CGFloat)g_ime_crect[3];
            NSRect sr = NSMakeRect(sx, sy, (CGFloat)g_ime_crect[2], (CGFloat)g_ime_crect[3]);
            if (g_ime_trace()) {
                fprintf(stderr, "[IME] firstRect caret -> %.0f,%.0f %.0fx%.0f\n",
                        sr.origin.x, sr.origin.y, sr.size.width, sr.size.height);
            }
            return sr;
        }
    }
    return ((NSRect (*)(id, SEL, NSRange, NSRangePointer))objc_msgSend)(self,
        sel_registerName("ime_swz_firstRect:actualRange:"), range, actual);
}

// ---- C API ----

const char* gui_ime_preedit(void) {
    return g_ime_pre;
}

int gui_ime_has_preedit(void) {
    return g_ime_has_pre;
}

int gui_ime_set_caret(int x, int y, int w, int h) {
    g_ime_crect[0] = x;
    g_ime_crect[1] = y;
    g_ime_crect[2] = w;
    g_ime_crect[3] = h;
    return 0;
}

int gui_ime_caret(int* x, int* y, int* w, int* h) {
    if (g_ime_crect[2] <= 0 || g_ime_crect[3] <= 0) {
        return 0;
    }
    if (x) { *x = g_ime_crect[0]; }
    if (y) { *y = g_ime_crect[1]; }
    if (w) { *w = g_ime_crect[2]; }
    if (h) { *h = g_ime_crect[3]; }
    return 1;
}

// 显式初始化(gui_clay_init 尾调用一次;类在 GLFW 镜像加载时已注册)。
// 法 = class_addMethod 挂 C imp(swz 名,type encoding 取自原方法——NSRange
// 结构体参数编组必须同形)再 method_exchangeImplementations。
void gui_ime_swizzle(void) {
    if (g_ime_swizzled) {
        return;
    }
    g_ime_swizzled = 1;
    Class cls = objc_getClass("GLFWContentView");
    if (cls == NULL) {
        if (g_ime_trace()) { fprintf(stderr, "[IME] GLFWContentView 未注册\n"); }
        return;
    }
    // 组词镜像
    Method m1 = class_getInstanceMethod(cls, sel_registerName("setMarkedText:selectedRange:replacementRange:"));
    if (m1 != NULL) {
        SEL sn = sel_registerName("ime_swz_setMarkedText:selectedRange:replacementRange:");
        class_addMethod(cls, sn, (IMP)ime_swz_setMarkedText, method_getTypeEncoding(m1));
        Method s1 = class_getInstanceMethod(cls, sn);
        if (s1 != NULL) {
            method_exchangeImplementations(m1, s1);
        }
    }
    // 候选窗定位
    Method m2 = class_getInstanceMethod(cls, sel_registerName("firstRectForCharacterRange:actualRange:"));
    if (m2 != NULL) {
        SEL sn2 = sel_registerName("ime_swz_firstRect:actualRange:");
        class_addMethod(cls, sn2, (IMP)ime_swz_firstRect, method_getTypeEncoding(m2));
        Method s2 = class_getInstanceMethod(cls, sn2);
        if (s2 != NULL) {
            method_exchangeImplementations(m2, s2);
        }
    }
    // 提交/解组清镜像
    Method m3 = class_getInstanceMethod(cls, sel_registerName("insertText:replacementRange:"));
    if (m3 != NULL) {
        SEL sn3 = sel_registerName("ime_swz_insertText:replacementRange:");
        class_addMethod(cls, sn3, (IMP)ime_swz_insertText, method_getTypeEncoding(m3));
        Method s3 = class_getInstanceMethod(cls, sn3);
        if (s3 != NULL) {
            method_exchangeImplementations(m3, s3);
        }
    }
    Method m4 = class_getInstanceMethod(cls, sel_registerName("unmarkText"));
    if (m4 != NULL) {
        SEL sn4 = sel_registerName("ime_swz_unmarkText");
        class_addMethod(cls, sn4, (IMP)ime_swz_unmarkText, method_getTypeEncoding(m4));
        Method s4 = class_getInstanceMethod(cls, sn4);
        if (s4 != NULL) {
            method_exchangeImplementations(m4, s4);
        }
    }
    if (g_ime_trace()) { fprintf(stderr, "[IME] swizzle done\n"); }
}

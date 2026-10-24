;;; ctron-mode.el --- Ctron (.ct) major mode -*- lexical-binding: t; -*-
;; Version: 0.1.0
;; Package-Requires: ((emacs "24.3"))
;; 权威依据:docs/spec/01-lexical-grammar.md §1.2–§1.5
;;
;; 范围:语法高亮 + 注释命令 + 文件关联。
;; 已知缺口(见 editors/emacs-ctron/README.md):§1.6 感知缩进、插值内子着色、
;; GUI view/style 标记着色 —— 在无 Emacs 的宿主机上开发,未运行验证;
;; 启用前请先字节编译:emacs --batch -f batch-byte-compile ctron-mode.el

(require 'font-lock)

(defgroup ctron nil
  "Ctron(.ct)编辑支持。"
  :prefix "ctron-"
  :group 'languages)

(defcustom ctron-indent-offset 4
  "缩进宽度(保留供 §1.6 缩进实现使用)。"
  :type 'integer)

(defconst ctron-mode-syntax-table
  (let ((st (make-syntax-table)))
    ;; §1.2:仅行注释(// 与 ///),无块注释
    (modify-syntax-entry ?/ ". 124" st)
    (modify-syntax-entry ?\n "> b" st)
    ;; 双引号字符串;\ 为转义对字符
    (modify-syntax-entry ?\" "\"" st)
    (modify-syntax-entry ?\\ "\\" st)
    ;; §1.3:下划线属词法字符
    (modify-syntax-entry ?_ "w" st)
    st)
  "Ctron 语法表。")

(defconst ctron-keywords
  '("fn" "let" "var" "const" "static" "comptime"
    "if" "else" "match" "while" "for" "in" "return"
    "struct" "class" "enum" "trait" "impl"
    "own" "scope" "test" "use" "pub" "extern"
    "prop" "break" "continue" "or")
  "§1.3 关键字全集(break/continue 为 v0.7 转正形态)。")

(defconst ctron-types
  '("Bool" "I8" "I16" "I32" "I64" "ISize" "U8" "U16" "U32" "U64" "USize"
    "F32" "F64" "Str" "String" "Void"
    "Option" "Result" "List" "Box" "Channel" "Mutex" "Atomic" "Global"
    "Arena" "Task" "Simd" "AnyError")
  "内建与前奏类型(§3.8 前奏 API 面)。")

(defconst ctron-font-lock-keywords
  (list
   ;; 非法标记标红(§1.3:禁用 ; 与 ::、无块注释;char 字面量不存在)
   (cons (rx (or ";" "::" "/*")) 'font-lock-warning-face)
   ;; 关键字
   (cons (rx-to-string
          `(and symbol-start (or ,@ctron-keywords) symbol-end))
         'font-lock-keyword-face)
   ;; 类型与前奏类型
   (cons (rx-to-string
          `(and symbol-start (or ,@ctron-types) symbol-end))
         'font-lock-type-face)
   ;; PascalCase 用户类型启发式(同 vscode grammar;SCREAMING 常量同色)
   (cons (rx symbol-start
             (group (any "A-Z") (* (any "A-Za-z0-9_")))
             symbol-end)
         '(1 font-lock-type-face))
   ;; 布尔/void
   (cons (rx symbol-start (or "true" "false" "void") symbol-end)
         'font-lock-constant-face)
   ;; self
   (cons (rx symbol-start "self" symbol-end)
         'font-lock-builtin-face)
   ;; 数字(进制前缀/下划线/浮点指数/整型浮点后缀,§1.4)
   (cons (rx symbol-start
             (or (and "0x" (+ (any "0-9a-fA-F_")))
                 (and "0o" (+ (any "0-7_")))
                 (and "0b" (+ (any "01_")))
                 (and (+ (any "0-9"))
                      (opt (and "." (+ (any "0-9_"))))
                      (opt (and (any "eE") (opt (any "+-"))) (+ (any "0-9_")))))
             (opt (or "i8" "i16" "i32" "i64" "isize"
                      "u8" "u16" "u32" "u64" "usize" "f32" "f64"))
             symbol-end)
         'font-lock-constant-face)
   ;; fn 函数名
   (cons (rx symbol-start "fn" (+ space)
             (group (any "A-Za-z_") (* (any "A-Za-z0-9_"))))
         '(1 font-lock-function-name-face)))
  "Ctron 高亮规则。字符串字面量由语法表驱动 font-lock 通用规则。")

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.ct\\'" . ctron-mode))

;;;###autoload
(define-derived-mode ctron-mode prog-mode "Ctron"
  "Ctron(.ct)主要模式:语法高亮 + 注释命令。
\\{ctron-mode-map}"
  (setq-local comment-start "// ")
  (setq-local comment-end "")
  (setq-local comment-start-skip "//+\\s-*")
  ;; §1.6 感知缩进待实现:先用标准相对缩进
  (setq-local indent-line-function #'indent-relative)
  (setq font-lock-defaults '(ctron-font-lock-keywords nil nil ((?_ . "w")))))

(provide 'ctron-mode)
;;; ctron-mode.el ends here

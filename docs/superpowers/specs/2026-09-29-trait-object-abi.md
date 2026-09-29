# Ctron Trait 对象 ABI 设计(T24;§3.4/§3.5)

> 状态:设计定稿(2026-09-29)。实施=T25(发射侧动态分发 codegen)。
> 消费场景:能力对象注入(§8.1 &Fs/&Clock)、`&Trait` 参数(§3.5)、超 trait 上下文组合(§8.1)。

## 1. 对象表示

`&Trait` = **胖指针**(fat pointer):

```c
typedef struct {
    void* data;          // 具体类型实例指针(值类型 → 堆盒;class → 引用)
    void* vtable;        // 虚表指针(指向该 trait+类型的静态 vtable)
} ct_trait_obj;
```

- **上行转换**(`&FakeClock` → `&Clock`):零成本——构造胖指针 `{&instance, &vtable_Clock_for_FakeClock}`;编译期已知，无运行时查找。
- **值类型装箱**:struct 值经 `ctron_amalloc` 堆盒化(同 Box 装箱路径);class 引用直接传。
- **不可空**(§3.5):data/vtable 恒非 NULL;无 Option[&Trait] 糖。

## 2. 虚表布局

```c
// 以 trait Clock { fn now(&self) -> U64; prop name: Str } 为例
typedef struct {
    // 头部(所有 vtable 共享前缀;超 trait 复用)
    const char* trait_name;    // 调试/Send 位承载
    int          send_flag;    // §7.4 动态 Send 位(具体类型的 Send 属性)
    // 方法槽(按 trait 声明序;含默认方法)
    U64         (*now)(void* self);
    // prop 槽(跟在方法槽后;getter 形)
    const char* (*name_get)(void* self);
} vt_Clock;
```

裁决项:
- **prop = getter 槽**:prop 在虚表中以零参 getter 函数表示(调用无括号语法糖 → getter 调用)。
- **方法槽序 = trait 声明序**(非实现序)——上行转换后槽号恒定。
- **默认方法**:有默认体的方法在 vtable 中有槽;impl 可覆盖(覆盖时槽指向 impl 方法)。
- **泛型 trait 对象**:v2(泛型 trait 的每个实例化各自有 vtable;v1 只支持非泛型)。

## 3. 超 trait 复用

```c
trait Env: Clock + Fs { fn log(&self) }
// vt_Env 前缀 = vt_Clock || vt_Fs(拼接),后接 Env 自身方法槽
```

- 超 trait 的 vtable 内容**嵌入**(非指针链接)——单级间接、缓存友好。
- `&Env` 上行到 `&Clock`:取 vt_Env 的前缀段指针(零成本偏移)。

## 4. 分发调用

```c
// Ctron: fn tick(c: &Clock) { let t = c.now() }
// 发射:
static U64 t_tick(ct_trait_obj c) {
    U64 t = ((vt_Clock*)c.vtable)->now(c.data);
    return t;
}
```

- **单级间接**:vtable 槽直接函数指针调用(非多级查找)。
- **接收者传 data 指针**:方法内 self 绑定 = `*(t_<Type>*)data`(值类型)或 `data`(class)。

## 5. Send 位

- vtable 头部 `send_flag` 承载具体类型的 Send 属性(编译期计算)。
- `&Trait` 恒非 Send(§7.4 v0.3 保守裁决);`send_flag` 为 v2 动态 Send 位预留。

## 6. 与既有面交互

| 面 | 交互 |
|---|---|
| E3031(非 Send 静态存储) | `&Trait` 恒非 Send → 不可作 static/Global;检查面不变 |
| E4042(C-ABI 回调禁捕获) | `&Trait` 含函数指针(vtable),不可跨 C 边界;W8052 面 |
| Box[T] | 同为堆盒但单类型;`&Trait` 是多态盒+虚表(不共享表示) |
| UFCS | `recv.m(a)` 解析顺序:类型固有方法 → **trait impl 方法(经 vtable)** → 前奏 |

## 7. 实施路径(T25)

1. **sem**: `&Trait` 形参类型解析(Ref(Named(trait)) 型码);方法调用经 vtable 分发(非 UFCS)。
2. **发射**: vtable 常量发射(`static const vt_<Trait> vt_<Trait>_for_<Type>`);
   `&x` → `&Trait` 构造(装箱+vtable 取址);`.m()` → `((vt_T*)obj.vtable)->m(obj.data)`。
3. **解释器**: 既有 eval_trait 方法查表已是动态分发;`&Trait` 参数绑定即可。

## 8. 不做面(v1)

- 泛型 trait 对象(v2)
- `Box[&Trait]`(owned trait object;规范预留项)
- 动态 Send 位(v2)
- downcast(向下转换;无 `as` 面需求)

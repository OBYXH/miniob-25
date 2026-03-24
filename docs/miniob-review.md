# MiniOB 项目复习文档

> OceanBase 2025 数据库大赛初赛 - 全面技术复习指南
> 适用于实习面试准备

---

## 目录

1. [项目背景与概述](#1-项目背景与概述)
2. [系统整体架构](#2-系统整体架构)
3. [SQL 完整执行链路](#3-sql-完整执行链路)
4. [核心数据结构](#4-核心数据结构)
5. [存储引擎设计](#5-存储引擎设计)
6. [索引设计（B+ Tree）](#6-索引设计b-tree)
7. [事务与并发控制（MVCC）](#7-事务与并发控制mvcc)
8. [我的主要工作详解](#8-我的主要工作详解)
   - 8.1 DROP TABLE
   - 8.2 LIKE 操作符
   - 8.3 多列索引（Multi-Index）
   - 8.4 唯一索引（Unique-Index）
   - 8.5 TEXT 大字段类型
   - 8.6 MVCC 实现细节
   - 8.7 全文索引（Full-Text Index）
9. [队友工作分析](#9-队友工作分析)
   - 9.1 UPDATE 语句
   - 9.2 DATE 类型
   - 9.3 表达式（Expression）
   - 9.4 GROUP BY 聚合
   - 9.5 别名（Alias）
   - 9.6 ORDER BY 排序
   - 9.7 向量类型（VECTOR）
   - 9.8 ALTER TABLE
   - 9.9 视图（VIEW）
   - 9.10 UNION 联合查询
   - 9.11 JOIN 连接查询
10. [关键设计难点与问题](#10-关键设计难点与问题)
11. [面试常见问题与答案](#11-面试常见问题与答案)
12. [重要知识点总结](#12-重要知识点总结)

---

## 1. 项目背景与概述

### 1.1 MiniOB 是什么

MiniOB 是 OceanBase 团队基于华中科技大学数据库课程原型，联合多所高校重新开发的数据库入门学习项目。整体代码简洁，容易上手，包含了数据库的各个关键模块：

- **存储引擎**：基于磁盘的 B+ Tree 索引、堆表存储
- **查询优化**：规则化重写、物理计划生成
- **事务管理**：MVCC 多版本并发控制
- **SQL 解析**：Flex + Bison 词法/语法分析
- **元数据管理**：数据库、表、字段、索引的元数据存储

### 1.2 比赛题目概览

2025年初赛共有以下题目类别（我队完成情况）：

| 题目 | 类别 | 完成方 | 难度 |
|------|------|--------|------|
| drop-table | DDL | 我 | ★ |
| like | DML | 我 | ★★ |
| multi-index | DDL | 我 | ★★★ |
| unique-index | DDL | 我 | ★★★ |
| text | DDL/DML | 我 | ★★ |
| mvcc | 事务 | 我 | ★★★★ |
| full-text-index | 全文搜索 | 我 | ★★★★ |
| update | DML | 队友 | ★★ |
| date | DDL/DML | 队友 | ★★ |
| expression | DML | 队友 | ★★★ |
| group-by | DML | 队友 | ★★★ |
| alias | DML | 队友 | ★★ |
| order-by | DML | 队友 | ★★★ |
| vector | DDL/DML | 队友 | ★★★★ |
| alter | DDL | 队友 | ★★★ |
| view | DDL/DML | 队友 | ★★★★ |
| union | DML | 队友 | ★★★ |
| join | DML | 队友 | ★★★ |

### 1.3 技术栈

- **语言**: C++17/20
- **构建**: CMake
- **词法分析**: Flex（lex_sql.l）
- **语法分析**: Bison/Yacc（yacc_sql.y）
- **分词器**: cppjieba（结巴分词）
- **协议**: 自定义文本协议 / MySQL Protocol

---

## 2. 系统整体架构

### 2.1 架构图解

```
客户端 (obclient/MySQL Client)
          |
          | TCP/Unix Socket
          |
   ┌──────▼──────────────────────────────────────────┐
   │              网络层 (NET Service)                │
   │  - 连接管理 (ThreadPool / OneConnection)         │
   │  - 协议解析 (Text Protocol / MySQL Protocol)     │
   └──────────────────┬──────────────────────────────┘
                      |
   ┌──────────────────▼──────────────────────────────┐
   │              会话层 (Session)                    │
   │  - 会话状态管理                                   │
   │  - 事务绑定                                       │
   └──────────────────┬──────────────────────────────┘
                      |
   ┌──────────────────▼──────────────────────────────┐
   │           SQL 处理流水线                          │
   │  ┌──────────┐  ┌──────────┐  ┌──────────────┐   │
   │  │ 解析阶段  │→ │ 语义解析 │→ │   优化阶段   │   │
   │  │ (Parse)  │  │(Resolve) │  │  (Optimize)  │   │
   │  └──────────┘  └──────────┘  └──────────────┘   │
   │                                      |            │
   │                              ┌──────▼──────┐     │
   │                              │  执行阶段    │     │
   │                              │  (Execute)  │     │
   │                              └─────────────┘     │
   └──────────────────────────────────────────────────┘
                      |
   ┌──────────────────▼──────────────────────────────┐
   │              存储引擎层                           │
   │  ┌─────────────┐  ┌────────────┐  ┌──────────┐  │
   │  │  Record     │  │  B+ Tree   │  │ Buffer   │  │
   │  │  Manager    │  │  Index     │  │  Pool    │  │
   │  └─────────────┘  └────────────┘  └──────────┘  │
   │  ┌─────────────────────────────────────────────┐ │
   │  │         元数据管理 (TableMeta, FieldMeta)    │ │
   │  └─────────────────────────────────────────────┘ │
   └──────────────────────────────────────────────────┘
                      |
   ┌──────────────────▼──────────────────────────────┐
   │           事务与日志层                            │
   │  ┌──────────┐  ┌──────────────┐                 │
   │  │   MVCC   │  │  Redo Log    │                 │
   │  │   Trx    │  │  (CLog)      │                 │
   │  └──────────┘  └──────────────┘                 │
   └──────────────────────────────────────────────────┘
```

### 2.2 核心模块说明

| 模块 | 文件位置 | 职责 |
|------|----------|------|
| 网络模块 | `src/observer/net/` | 接受客户端连接，收发请求 |
| SQL 解析器 | `src/observer/sql/parser/` | 词法+语法分析，生成 AST |
| 语义解析 | `src/observer/sql/stmt/` | AST → Stmt，验证元数据 |
| 查询优化 | `src/observer/sql/optimizer/` | 生成逻辑/物理执行计划 |
| 执行器 | `src/observer/sql/executor/` | DDL 命令执行 |
| 算子树 | `src/observer/sql/operator/` | DML 火山模型算子 |
| 表引擎 | `src/observer/storage/table/` | 表级操作（增删改查） |
| 记录管理 | `src/observer/storage/record/` | 页内记录的物理存储 |
| 索引 | `src/observer/storage/index/` | B+ Tree、全文索引 |
| 缓冲池 | `src/observer/storage/buffer/` | 内存页管理 |
| 事务 | `src/observer/storage/trx/` | MVCC 事务管理 |
| 日志 | `src/observer/storage/clog/` | Redo Log |

---

## 3. SQL 完整执行链路

### 3.1 执行流水线详解

```
SQL String
    |
    ▼ [ParseStage]
ParsedSqlNode (AST)
    - 字段名: string
    - 表名: string
    - 原始条件节点
    |
    ▼ [ResolveStage]  → Stmt::create_stmt()
Stmt 对象 (SelectStmt/InsertStmt/...)
    - 字段: FieldMeta*
    - 表: Table*
    - 条件: Expression*
    |
    ▼ [OptimizeStage]
    ├── LogicalPlanGenerator → LogicalOperator Tree
    ├── Rewriter (规则重写)
    │   ├── ComparisonSimplificationRule
    │   ├── ConjunctionSimplificationRule
    │   └── PredicatePushdownRewriter
    └── PhysicalPlanGenerator → PhysicalOperator Tree
    |
    ▼ [ExecuteStage]
    ├── DDL: CommandExecutor → 具体 Executor
    └── DML: PhysicalOperator.open/next/close()
    |
    ▼
Result / SqlResult
    |
    ▼ [Protocol Layer]
Client Response
```

### 3.2 两种执行路径

#### Path A：DDL 命令路径

```
ParsedSqlNode(CREATE INDEX)
    → ResolveStage → CreateIndexStmt
    → ExecuteStage → CommandExecutor
    → CreateIndexExecutor::execute()
    → Table::create_index()
    → HeapTableEngine::create_index()
    → BplusTreeIndex::create()
```

#### Path B：DML 查询路径（火山模型）

```
ParsedSqlNode(SELECT)
    → ResolveStage → SelectStmt
    → OptimizeStage
        → LogicalPlanGenerator → TableGetLogicalOperator
                                  → PredicateLogicalOperator
                                  → ProjectLogicalOperator
        → PhysicalPlanGenerator → TableScanPhysicalOperator / IndexScanPhysicalOperator
                                   → PredicatePhysicalOperator
                                   → ProjectPhysicalOperator
    → ExecuteStage → SqlResult.set_operator(root_physical_op)
    → 客户端迭代 root_physical_op.next()
```

### 3.3 火山模型（Volcano Model）

MiniOB 使用经典火山模型：每个算子实现 `open()`, `next()`, `close()` 接口。

```cpp
class PhysicalOperator {
  virtual RC open(Trx *trx) = 0;    // 初始化，递归打开子算子
  virtual RC next() = 0;             // 获取下一行，RECORD_EOF 表示结束
  virtual RC close() = 0;            // 关闭，释放资源
  virtual Tuple* current_tuple() = 0; // 获取当前行数据
  vector<unique_ptr<PhysicalOperator>> children_;
};
```

**执行示例（SELECT * FROM t WHERE id > 5 ORDER BY name）**：

```
OrderByPhysicalOperator
    └── PredicatePhysicalOperator (id > 5)
            └── TableScanPhysicalOperator (scan table t)
```

1. `OrderBy.open()` → `Predicate.open()` → `TableScan.open()`
2. `OrderBy.next()` → 拉取所有行并排序
3. `TableScan.next()` → 遍历每条记录 → 返回 RowTuple
4. `Predicate` 过滤 id > 5 的记录
5. `OrderBy` 积攒所有结果后输出有序序列

### 3.4 关键文件索引

| 阶段 | 关键文件 |
|------|----------|
| 词法分析 | `sql/parser/lex_sql.l` |
| 语法分析 | `sql/parser/yacc_sql.y` |
| AST 节点 | `sql/parser/parse_defs.h` |
| Stmt 基类 | `sql/stmt/stmt.h` |
| 逻辑计划 | `sql/optimizer/logical_plan_generator.h/cpp` |
| 物理计划 | `sql/optimizer/physical_plan_generator.h/cpp` |
| 重写规则 | `sql/optimizer/rewriter.h/cpp` |
| 命令执行 | `sql/executor/execute_stage.cpp` |
| DDL 执行 | `sql/executor/command_executor.h` |

---

## 4. 核心数据结构

### 4.1 Value 类型系统

`Value` 是 MiniOB 中表示单个值的核心类，使用 union 存储不同类型数据：

```cpp
class Value final {
private:
  AttrType attr_type_ = AttrType::UNDEFINED;
  int      length_    = 0;
  union Val {
    int32_t  int_value_;
    float    float_value_;
    bool     bool_value_;
    char    *pointer_value_;    // for CHARS, TEXTS
    float   *vector_value_;     // for VECTORS
  } value_ = {.int_value_ = 0};
  bool own_data_ = false;       // 是否拥有内存所有权
  bool is_null_  = false;       // NULL 标记
};
```

**支持的类型（AttrType）**：
- `UNDEFINED`: 未定义
- `CHARS`: 字符串（定长）
- `INTS`: 整数（4字节）
- `FLOATS`: 浮点数（4字节）
- `DATES`: 日期（存储为 YYYYMMDD 整数）
- `VECTORS`: 浮点数组（向量）
- `BOOLEANS`: 布尔值
- `NULLS`: 空值
- `TEXTS`: 大文本（最长65535）

**关键方法**：
- `set_xxx()/get_xxx()`: 类型安全的值设置与获取
- `cast_to(target_type, result)`: 类型转换（调用 DataType 子类）
- `compare(other)`: 通过 DataType 子类实现比较
- `to_string()`: 转为字符串表示

### 4.2 Expression 表达式体系

```cpp
enum class ExprType {
  NONE,
  STAR,                  // SELECT *
  UNBOUND_FIELD,         // 未绑定字段引用（解析前）
  FIELD,                 // 已绑定字段引用（FieldExpr）
  VALUE,                 // 常量值（ValueExpr）
  CAST,                  // 类型转换
  COMPARISON,            // 比较运算（ComparisonExpr）
  CONJUNCTION,           // 逻辑与/或（ConjunctionExpr）
  ARITHMETIC,            // 算术运算（ArithmeticExpr）
  AGGREGATION,           // 聚合函数（AggregateExpr）
  FUNCTION,              // 内置函数（FunctionExpr）
  VEC_DISTANCE,          // 向量距离（VecDistanceExpr）
  SUBQUERY,              // 子查询（SubqueryExpr）
  VALUE_LIST,            // 值列表（ValueListExpr）
  // ...
};

class Expression {
public:
  virtual RC get_value(const Tuple &tuple, Value &value) = 0; // 从 tuple 中求值
  virtual ExprType type() = 0;
  string name_;      // 表达式名（用于结果集列名）
  string alias_;     // 别名
};
```

### 4.3 Tuple / TupleSchema

```cpp
// 描述一行数据的结构（列名信息）
class TupleSchema {
  vector<TupleCellSpec> cells_;  // 每列的描述信息
};

// TupleCellSpec 包含：表名、字段名、别名

// Tuple 是行数据的抽象接口
class Tuple {
public:
  virtual int cell_num() const = 0;
  virtual RC  cell_at(int index, Value &cell) const = 0;
  virtual RC  find_cell(const TupleCellSpec &spec, Value &cell) const = 0;
  virtual const TupleSchema &schema() const = 0;
};
```

**Tuple 的主要实现**：
- `RowTuple`: 从 Record 读取，配合 FieldMeta 解析字段
- `ProjectTuple`: 包装子 Tuple，只暴露特定列
- `JoinedTuple`: 合并两个 Tuple（LEFT JOIN / INNER JOIN）
- `ExpressionTuple`: 用于计算表达式列

### 4.4 Record 与 RID

```cpp
// 行标识符
struct RID {
  PageNum page_num;  // 页号
  SlotNum slot_num;  // 页内槽号
};

// 一条物理记录
class Record {
  RID   rid_;    // 记录位置
  char *data_;   // 原始字节数据
  int   len_;    // 字节长度
  bool  owner_;  // 是否拥有内存
};
```

**记录格式**（堆表，固定长度记录）：

```
[trx_begin_xid (4B)] [trx_end_xid (4B)] [field1_data] [field2_data] ... [fieldN_data]
                      ^-- MVCC 使用的系统字段 --^
```

每个字段的布局由 `FieldMeta` 中的 `offset` 和 `len` 确定。

### 4.5 FieldMeta 与 TableMeta

```cpp
class FieldMeta {
  string   name_;        // 字段名
  AttrType attr_type_;   // 数据类型
  int      attr_offset_; // 在记录中的字节偏移
  int      attr_len_;    // 字节长度
  bool     visible_;     // 是否在结果集中显示
  int      field_id_;    // 全局唯一字段 ID
  bool     nullable_;    // 是否可以为 NULL
};

class TableMeta {
  int32_t           table_id_;
  string            name_;
  vector<FieldMeta> trx_fields_;  // MVCC 系统字段（begin/end xid）
  vector<FieldMeta> fields_;      // 所有字段（含系统字段）
  vector<IndexMeta> indexes_;     // 所有索引
  int               record_size_; // 记录总字节数
  StorageFormat     storage_format_;
  StorageEngine     storage_engine_;
};
```

### 4.6 IndexMeta

```cpp
class IndexMeta {
  string            name_;             // 索引名
  IndexType         index_type_;       // BPlusTreeIndex / FullTextIndex
  vector<FieldMeta> fields_;           // 索引列（支持多列）
  vector<int>       fields_offset_;    // 各列在索引 key 中的偏移
  int               fields_total_len_; // 索引 key 总长度
  bool              is_unique_;        // 是否唯一索引
};
```

---

## 5. 存储引擎设计

### 5.1 Buffer Pool（缓冲池）

MiniOB 使用固定大小（8192 字节）的页面管理磁盘数据：

```
磁盘文件
┌─────────┬─────────┬─────────┬─────────┐
│ Header  │ Page 1  │ Page 2  │ Page N  │
│ (页分配  │ (记录数 │         │         │
│  位图)  │  据)    │         │         │
└─────────┴─────────┴─────────┴─────────┘

内存 Buffer Pool
┌───────────────────────────────────────┐
│  Frame[] (固定数量的内存帧)            │
│  LRU 链表管理冷热页面                  │
│  Pin/Unpin 机制防止 eviction           │
└───────────────────────────────────────┘
```

**关键 API**：
- `bpm.open_file()` → 返回 `DiskBufferPool*`
- `pool.fetch_page(page_num)` → 返回 `Frame*`（内存中的页面）
- `pool.flush_page(frame)` → 写回磁盘
- `pool.allocate_page()` → 分配新页面

### 5.2 Record Manager

负责在页面中管理记录槽（Slot）：

```
一个数据页的布局：
┌──────────────────────────────────────────────────────┐
│ PageHeader: slot_count, record_size, bitmap_size     │
│ Bitmap: 标记每个 slot 是否被占用                     │
├──────────────────────────────────────────────────────┤
│ Slot 0: [record_data, record_size bytes]             │
│ Slot 1: [record_data, record_size bytes]             │
│ ...                                                   │
│ Slot N: [record_data, record_size bytes]             │
└──────────────────────────────────────────────────────┘
```

**RecordFileHandler 主要操作**：
- `insert_record(data, size, rid)`: 找空 slot 插入
- `delete_record(rid)`: 将 slot 标记为空
- `update_record(data, rid)`: 覆盖指定 rid 的数据
- `get_record(rid, record)`: 读取指定记录
- `scan()`: 全表扫描所有非空 slot

### 5.3 HeapTableEngine

```cpp
class HeapTableEngine : public TableEngine {
  TableMeta       *table_meta_;    // 表元数据指针
  Db              *db_;            // 数据库指针
  Table           *table_;         // 表对象指针
  DiskBufferPool  *data_buffer_pool_; // 数据文件的 Buffer Pool
  RecordFileHandler *record_handler_;  // Record 管理器
  vector<Index *>  indexes_;       // 所有索引
};
```

**insert_record 完整流程**：
```
1. record_handler_->insert_record() → 写入堆文件，获得 RID
2. insert_entry_of_indexes()         → 向所有索引插入 (key, rid)
3. 如果索引插入失败（如唯一约束冲突）:
   - delete_entry_of_indexes()        → 回滚索引
   - record_handler_->delete_record() → 回滚记录
```

---

## 6. 索引设计（B+ Tree）

### 6.1 B+ Tree 结构

```
                  [根节点 - 内部节点]
                 /                  \
        [内部节点]                 [内部节点]
        /       \                 /        \
  [叶子节点]  [叶子节点]  [叶子节点]  [叶子节点]
   ↔ (双向链表，支持范围扫描)
```

**叶子节点存储**：`(key, RID)` 对，key = 索引字段值 + RID（保证唯一性）

**内部节点存储**：`(key, child_page_num)` 对

### 6.2 索引文件格式

```
文件结构：
Page 0: IndexFileHeader
  - root_page: 根节点页号
  - attr_length: 属性总长度（字节）
  - key_length: attr_length + sizeof(RID)
  - internal_max_size: 内部节点最大键数
  - leaf_max_size: 叶子节点最大键数

Page N: LeafIndexNode / InternalIndexNode
  IndexNode Header (12 bytes):
    - is_leaf: bool
    - key_num: int
    - parent: PageNum

  LeafIndexNode (additional 4 bytes):
    - next_brother: PageNum (叶子链表指针)
```

### 6.3 多列索引 Key 结构

对于多列索引 `(field1, field2)`，key 的内存布局：

```
| field1_data (field1.len bytes) | field2_data (field2.len bytes) | RID (8 bytes) |
```

`KeyComparator::compare_key()` 按照字段顺序逐个比较：

```cpp
int compare_key(const char *v1, const char *v2) const {
  for (int i = 0; i < field_number; i++) {
    int offset = fields_offset_[i];
    // 处理 NULL 值（末尾标志位）
    if (field.nullable()) {
      bool v1_null = v1[offset + field.len() - 1] == '1';
      bool v2_null = v2[offset + field.len() - 1] == '1';
      if (v1_null) return -1;  // NULL 视为最小
      if (v2_null) return 1;
    }
    int result = attr_comparators_[i](v1 + offset, v2 + offset);
    if (result != 0) return result;
  }
  return 0; // 相等时不含 RID 比较
}
```

### 6.4 唯一索引实现

唯一索引在 `BplusTreeIndex::insert_entry()` 时需要额外检查：

```
1. 提取 key（不含 RID）
2. 用 BplusTreeScanner 在索引中查找 key
3. 如果找到匹配记录（且非 NULL），返回 RC::DUPLICATE_KEY
4. 否则正常插入
```

**NULL 的唯一性处理**：根据 SQL 标准，多个 NULL 值不被视为重复，因此唯一索引允许多个 NULL 值共存。这需要在唯一性检查时跳过 NULL key。

---

## 7. 事务与并发控制（MVCC）

### 7.1 MVCC 基本原理

每行记录包含两个系统字段（对用户透明）：

```
记录格式：
[begin_xid (4B)] [end_xid (4B)] [用户数据...]
```

- `begin_xid`: 创建该版本的事务 commit_id（正数=已提交，负数=-trx_id=未提交）
- `end_xid`: 删除该版本的事务 commit_id（正数=已提交，负数=-trx_id=未提交，+∞=未删除）

### 7.2 版本可见性规则

对于事务 T（trx_id），读取一条记录：

| begin_xid | end_xid | 状态 | T 能否看到 |
|-----------|---------|------|-----------|
| > 0 | > 0 | 已提交，有效版本 | begin_xid ≤ trx_id ≤ end_xid |
| < 0 | +∞ | 未提交插入 | -begin_xid == trx_id（自己插入的） |
| > 0 | < 0 | 未提交删除 | 只读：-end_xid ≠ trx_id；读写：冲突 |

```cpp
RC MvccTrx::visit_record(Table *table, Record &record, ReadWriteMode mode) {
  int32_t begin_xid = begin_field.get_int(record);
  int32_t end_xid   = end_field.get_int(record);

  if (begin_xid > 0 && end_xid > 0) {
    // 已提交版本：检查 trx_id 是否在版本范围内
    return (trx_id_ >= begin_xid && trx_id_ <= end_xid)
           ? RC::SUCCESS : RC::RECORD_INVISIBLE;
  }
  // ... 处理未提交情况
}
```

### 7.3 事务操作流程

#### INSERT

```
1. begin_field = -trx_id_  （未提交标记）
2. end_field   = max_trx_id（无穷大，表示未删除）
3. table->insert_record()
4. log_handler_.insert_record() 记录日志
5. operations_.emplace_back(INSERT, table, rid)
```

#### DELETE

```
1. table->visit_record() 检查可见性
2. end_field = -trx_id_    （标记为此事务删除，未提交）
3. log_handler_.delete_record()
4. operations_.push_back(DELETE, table, rid)
```

#### UPDATE

```
MiniOB MVCC 的 UPDATE = DELETE old + INSERT new：
1. visit_record() 检查 old_record 可见性
2. new_record 的 begin_field = -trx_id_, end_field = max
3. table->update_record(old, new)  // 物理替换
4. log_handler_.update_record()
5. operations_.emplace_back(UPDATE, table, old_rid, old_record, new_record)
```

#### COMMIT

```
生成 commit_xid（新的全局 ID）

对每个操作：
- INSERT: begin_field 从 -trx_id 改为 commit_xid
- DELETE: end_field   从 -trx_id 改为 commit_xid
- UPDATE: begin_field 从 -trx_id 改为 commit_xid
```

#### ROLLBACK

```
逆序遍历 operations_：
- INSERT: 物理删除该记录
- DELETE: end_field 从 -trx_id 改回 max_trx_id
- UPDATE: 物理恢复 old_record（swap 回来）
```

### 7.4 冲突处理

MiniOB 使用最简单的冲突处理策略：

```
如果读写操作发现记录被另一事务"锁定"（begin/end_xid 为负），
直接返回 RC::LOCKED_CONCURRENCY_CONFLICT，让客户端重试。
```

这是非常保守的悲观策略，避免了实现等待锁的复杂性。

---

## 8. 我的主要工作详解

### 8.1 DROP TABLE

#### 实现步骤

1. **词法分析** (`lex_sql.l`): `DROP TABLE` 关键字已有（基础代码）
2. **语法规则** (`yacc_sql.y`): 已有 `drop_table_stmt` 规则
3. **Stmt** (`drop_table_stmt.h/cpp`): 创建 `DropTableStmt`
4. **Executor** (`drop_table_executor.cpp`): 实现具体删除逻辑

#### 核心实现（Table::drop）

```cpp
RC Table::drop() {
  RC rc = sync();  // 1. 先将数据刷盘

  rc = engine_->drop();  // 2. 关闭并删除索引文件

  // 3. 删除 meta 文件
  filesystem::remove(table_meta_file(base_dir_, name()));

  // 4. 删除 data 文件
  filesystem::remove(table_data_file(base_dir_, name()));

  // 5. 删除所有索引文件
  for (int i = 0; i < table_meta_.index_num(); i++) {
    auto index_name = table_meta_.index(i)->name();
    filesystem::remove(table_index_file(base_dir_, name(), index_name));
  }
  return RC::SUCCESS;
}
```

#### DB 层的调用

`Db::drop_table()` 还需要：
- 关闭并删除 Buffer Pool 中对应表的所有页面（防止内存泄漏）
- 从 `tables_` map 中移除表的记录

#### 涉及的知识点

- 文件系统操作（`std::filesystem`）
- 资源管理顺序（先 sync，再关闭，再删除文件）
- 需要同时清理内存中的元数据

---

### 8.2 LIKE 操作符

#### 实现步骤

1. **词法** (`lex_sql.l`): 添加 `LIKE` 关键字（基础代码已有）
2. **语法** (`yacc_sql.y`): `comp_op` 中添加 `LIKE_OP` 和 `NOT_LIKE_OP`
3. **比较运算** (`value.cpp`): 实现 `Value::LIKE()` 方法

#### Value::LIKE() 实现

```cpp
bool Value::LIKE(const Value &other) const {
  const string  left_str  = this->get_string();
  const string &right_str = other.get_string();

  // 将 SQL LIKE 模式转换为正则表达式
  string regex_str = std::regex_replace(right_str, std::regex("%"), ".*");
  regex_str        = std::regex_replace(regex_str, std::regex("_"),  ".");

  std::regex regex_pattern(regex_str);
  return std::regex_match(left_str, regex_pattern);
}
```

**LIKE 模式说明**：
- `%` → 匹配任意多个字符（对应正则 `.*`）
- `_` → 匹配任意单个字符（对应正则 `.`）

#### Expression 中的比较处理

在 `ComparisonExpr::get_value()` 中添加 LIKE 处理：

```cpp
case CompOp::LIKE_OP:
  result.set_boolean(left_value.LIKE(right_value));
  break;
case CompOp::NOT_LIKE_OP:
  result.set_boolean(!left_value.LIKE(right_value));
  break;
```

#### 潜在问题

- 正则表达式转换：如果模式包含正则特殊字符（如 `.`, `[`, `]`），需要先转义
- 大小写敏感性：SQL LIKE 在不同数据库中行为不同，MySQL 默认不区分大小写
- 性能问题：`std::regex` 每次构建开销较大，可以缓存编译后的 regex

---

### 8.3 多列索引（Multi-Index）

#### 问题背景

原始 MiniOB 仅支持**单列**索引。需要扩展 `CREATE INDEX` 语法和实现，使其支持 `CREATE INDEX idx ON t(a, b, c)` 这样的多列索引。

#### 语法扩展

```yacc
create_index_stmt:
    CREATE opt_unique INDEX ID ON ID LBRACE attr_list RBRACE
    {
      ...
      create_index.attribute_name.swap(*$8);  // attr_list 是 vector<string>
    }
```

`attr_list` 的递归语法已支持多列：
```yacc
attr_list:
    ID { $$ = new vector<string>(); $$->emplace_back($1); }
    | ID COMMA attr_list {
      $$ = $3;
      $$->emplace($$->begin(), $1);
    }
```

#### IndexMeta 设计

多列索引的关键在于 `IndexMeta` 中的 `fields_` 列表和 `fields_offset_`：

```
索引 (a, b) 的 key 内存布局：
| a_data (a.len bytes) | b_data (b.len bytes) | RID (8 bytes) |
  ^                      ^
  offset[0]=0            offset[1]=a.len
```

```cpp
RC IndexMeta::init(const string &name, IndexType type,
                   const vector<FieldMeta> &fields, bool unique) {
  name_     = name;
  is_unique_ = unique;
  fields_   = fields;

  // 计算每列在 key 中的偏移
  int offset = 0;
  for (const auto &field : fields_) {
    fields_offset_.push_back(offset);
    offset += field.len();
  }
  fields_total_len_ = offset;  // 不含 RID 的 key 总长度
}
```

#### CreateIndexStmt 扩展

```cpp
class CreateIndexStmt : public Stmt {
  Table           *table_;        // 目标表
  vector<FieldMeta> field_meta_;  // 所有索引列的元数据
  string           index_name_;
  bool             unique_;
  IndexType        index_type_;
};
```

**create_stmt() 中验证多列索引**：
```cpp
for (const string &field_name : create_index_node.attribute_name) {
  const FieldMeta *field = table->table_meta().field(field_name.c_str());
  if (field == nullptr) return RC::SCHEMA_FIELD_NOT_EXIST;
  field_metas.push_back(*field);
}
```

#### 建立索引时的记录扫描

创建索引时需要遍历已有数据，从每条记录中提取多列 key：

```cpp
// HeapTableEngine::create_index() 中
while (scanner->next(record) == RC::SUCCESS) {
  // record.data() 包含完整记录
  // index_meta 知道每列的 offset，可以直接提取 key
  rc = index->insert_entry(record.data(), &record.rid());
}
```

`BplusTreeIndex::insert_entry()` 会根据 `IndexMeta` 中的 `fields_offset_` 从记录数据中提取各列的值，拼接成 key 后插入 B+ Tree。

---

### 8.4 唯一索引（Unique-Index）

#### 与普通索引的区别

唯一索引在插入时需要检查唯一性约束：

```
普通索引 key：field_value + RID  （RID 保证全局唯一，无重复 key）
唯一索引检查：只比较 field_value 部分，不含 RID
```

#### 实现方式

在 `BplusTreeHandler::insert_entry()` 中，对唯一索引增加预检步骤：

```cpp
RC BplusTreeHandler::insert_entry(const char *user_key, const RID *rid) {
  // 如果是唯一索引
  if (index_meta_.is_unique()) {
    // 检查 key（不含 RID）是否已存在
    // 使用 KeyComparator::compare_key()（不比较 RID 部分）
    list<RID> existing_rids;
    RC rc = get_entry(user_key, key_len, existing_rids);
    if (rc == RC::SUCCESS && !existing_rids.empty()) {
      // 还需要验证找到的记录是否真实存在（MVCC 场景下可能是已删除记录）
      return RC::DUPLICATE_KEY;
    }
  }
  // 正常插入
  // ...
}
```

#### NULL 值特殊处理

**SQL 标准**：唯一索引中多个 NULL 值是允许的（NULL ≠ NULL）。

实现时，在 KeyComparator 中判断 NULL：
- 如果字段 nullable，记录末尾有一个字节标志 `'1'` 表示该字段为 NULL
- 唯一性检查时，如果当前要插入的 key 中任一字段为 NULL，跳过唯一性检查

```cpp
// 在唯一性检查前，先检查是否有 NULL 字段
bool has_null = false;
for (auto &field : index_meta_.fields()) {
  if (field.nullable()) {
    // 检查末尾标志位
    if (user_key[offset + field.len() - 1] == '1') {
      has_null = true;
      break;
    }
  }
}
if (has_null) {
  // NULL 值，跳过唯一性检查，直接插入
  goto insert;
}
```

这一点与直觉不同，花费了相当多的调试时间才发现。

#### UPDATE 时的唯一约束

UPDATE 操作将 old_record 删除索引，再插入 new_record 时也需要检查唯一约束。如果 UPDATE 多行数据（如 `UPDATE t SET id = 3 - id`），应先批量删除所有旧索引，再批量插入新索引，避免中间状态冲突。

---

### 8.5 TEXT 大字段类型

#### 实现思路

TEXT 类型（最大 65535 字节）远超正常页面大小（8KB），理论上需要外部存储（LOB Storage）。但在竞赛时间有限的情况下，采用了简化实现：**直接将 TEXT 数据存储在 Value 的 string 中，通过增大通信 buffer 来支持大数据传输**。

#### 关键修改 1：通信 Buffer 大小

```cpp
// src/observer/common/ini_setting.h
// 原来是 8192，改为大值
#define SOCKET_BUFFER_SIZE 8192 * 1024  // 8MB
```

客户端和服务端之间通过这个 buffer 传输数据，如果 TEXT 数据超过这个大小会截断。

#### 关键修改 2：AttrType 添加 TEXTS

```cpp
enum class AttrType : int {
  UNDEFINED = 0,
  CHARS,
  INTS,
  FLOATS,
  DATES,
  VECTORS,
  BOOLEANS,
  NULLS,
  TEXTS,   // 新增
  MAXTYPE
};
```

#### 关键修改 3：DataType 子类 TextType

```cpp
// data_type.cpp
array<unique_ptr<DataType>, ...> DataType::type_instances_ = {
  // ...
  make_unique<TextType>(),  // 对应 AttrType::TEXTS
};
```

`TextType` 继承 `DataType`，实现 `compare()`, `to_string()`, `cast_to()` 等。

#### 关键修改 4：Value 的 TEXT 处理

```cpp
void Value::set_text(const char *s, int len) {
  reset();
  attr_type_ = AttrType::TEXTS;
  if (s != nullptr) {
    own_data_ = true;
    len = strnlen(s, len);
    value_.pointer_value_ = new char[len + 1];
    memcpy(value_.pointer_value_, s, len);
    value_.pointer_value_[len] = '\0';
    length_ = len;
  }
}
```

#### 关键修改 5：make_record 中的类型转换

在 `Table::make_record()` 中，当字段类型为 TEXTS 但插入值为 CHARS 时：

```cpp
if (field->type() == AttrType::TEXTS && value.attr_type() == AttrType::CHARS) {
  rc = real_value.borrow_text(value);  // 借用 CHARS 的内存，修改 attr_type_ 为 TEXTS
} else {
  rc = Value::cast_to(value, field->type(), real_value);
}
```

#### 注意事项

- TEXT 字段在记录中占 65535 字节空间，造成记录很大，每页能存的记录数很少
- 更合理的实现应使用外部存储（LOB Handler），只在记录中存储指针和长度
- 当前实现对大量 TEXT 数据的性能很差

---

### 8.6 MVCC 实现细节

#### 设计背景

MiniOB 的 MVCC 是竞赛初赛必须实现的功能，用于测试在并发场景下的数据一致性。测试方式是用多个连接同时执行 SQL，验证隔离性。

#### 两种事务模式

```cpp
// 启动服务器时通过 -t 参数指定
observer -t mvcc  // MVCC 模式
observer -t vacuous  // 无事务模式（默认）
```

对应 `TrxKit::Type`:
```cpp
enum class Type {
  VACUOUS,  // 无事务
  MVCC,     // 多版本并发控制
};
```

#### MVCC 系统字段

```cpp
// MvccTrxKit::init() 初始化两个系统字段
fields_ = vector<FieldMeta>{
  FieldMeta("__trx_xid_begin", AttrType::INTS, 0, 4, false/*invisible*/, -1, false),
  FieldMeta("__trx_xid_end",   AttrType::INTS, 0, 4, false/*invisible*/, -2, false),
};
```

这两个字段放在每条记录的最前面（偏移 0），对用户不可见（`visible_=false`）。

#### 事务 ID 管理

```cpp
class MvccTrxKit {
  atomic<int32_t> current_trx_id_ = 0;  // 全局事务 ID 计数器

  int32_t next_trx_id() { return ++current_trx_id_; }  // 原子自增
  int32_t max_trx_id()  { return INT32_MAX; }           // "无穷大"
};
```

事务有两个阶段的 ID：
- `trx_id_`: 开始时分配，用于操作中的版本标记（取负数）
- `commit_xid`: 提交时分配，用于最终的版本范围

#### 详细的可见性判断代码

```cpp
RC MvccTrx::visit_record(Table *table, Record &record, ReadWriteMode mode) {
  int32_t begin_xid = begin_field.get_int(record);
  int32_t end_xid   = end_field.get_int(record);

  if (begin_xid > 0 && end_xid > 0) {
    // 完全已提交的版本
    if (trx_id_ >= begin_xid && trx_id_ <= end_xid) {
      return RC::SUCCESS;  // 可见
    }
    return RC::RECORD_INVISIBLE;

  } else if (end_xid < 0) {
    // 被某事务标记删除（未提交）
    if (mode == ReadWriteMode::READ_ONLY) {
      // 只读：如果是自己删除的则不可见，否则可见（忽略别人的未提交删除）
      return (-end_xid != trx_id_) ? RC::SUCCESS : RC::RECORD_INVISIBLE;
    } else {
      // 读写：发现冲突，直接报错
      return RC::LOCKED_CONCURRENCY_CONFLICT;
    }

  } else if (begin_xid < 0) {
    // 某事务插入（未提交）
    if (-begin_xid == trx_id_) {
      return RC::SUCCESS;  // 自己插入的，可见
    }
    return RC::RECORD_INVISIBLE;  // 别人插入的，不可见
  }
}
```

#### Commit 过程的原子性问题

⚠️ **重要缺陷**：`commit_with_trx_id()` 逐行更新版本号，不是原子操作：

```
T1 插入 R1, R2, R3，开始提交：
commit_id = 100

Step1: R1 的 begin_xid 从 -trx_id 改为 100 ✓
Step2: R2 的 begin_xid 从 -trx_id 改为 100 ✓
Step3: R3 的 begin_xid 还是 -trx_id（还未修改）

此时如果另一个事务 T2 的 trx_id = 110：
- 能看到 R1, R2（因为 begin_xid=100 ≤ 110）
- 看不到 R3（begin_xid=-trx_id，不是 T2 的事务）
```

这是 MiniOB MVCC 的已知缺陷（代码注释中也有说明），竞赛中未要求完全修复。

---

### 8.7 全文索引（Full-Text Index）

#### 整体架构

```
用户 SQL：SELECT * FROM t WHERE MATCH(content) AGAINST ('数据库');

执行路径：
├── 解析：MATCH(content) AGAINST('数据库') → FunctionExpr(MATCH_AGAINST, field=content, format='数据库')
├── 求值：FunctionExpr::get_value(tuple, result)
│     ├── 获取 FieldExpr（content 字段）
│     ├── 找到表和字段名
│     ├── table->find_index_by_field("content") → FullTextIndex*
│     ├── ft_index->search("数据库") → vector<FullTextSearchResult>
│     └── 根据当前 tuple 的 RID 找到匹配的分数
└── WHERE 条件：score > 0 则保留该行（或根据阈值过滤）
```

#### 倒排索引数据结构

```cpp
struct PostingList {
  unordered_map<RID, int, RIDHash> postings;  // RID → 词频(TF)
};

class FullTextIndex : public Index {
  unordered_map<string, PostingList>  inverted_index_;  // token → PostingList
  unordered_map<RID, int, RIDHash>    doc_lengths_;     // RID → 文档词数
  double                              avg_doc_len_;     // 平均文档长度
  shared_ptr<Tokenizer>               tokenizer_;       // 分词器
};
```

#### BM25 评分算法

```
BM25(q, d) = Σ IDF(t) × TF_norm(t, d)

IDF(t) = log((N - df + 0.5) / (df + 0.5))
  N  = 总文档数
  df = 包含词 t 的文档数

TF_norm(t, d) = TF(t, d) × (K1 + 1) / (TF(t, d) + K1 × (1 - B + B × len(d) / avgdl))
  TF(t, d) = 词 t 在文档 d 中的频次
  len(d)   = 文档 d 的词数
  avgdl    = 平均文档词数
  K1 = 1.5（词频饱和系数）
  B  = 0.75（文档长度归一化系数）
```

#### insert_entry 实现

```cpp
RC FullTextIndex::insert_entry(const char *record, const RID *rid) {
  // 从 record 中提取字段数据
  auto field_meta = index_meta_.fields()[0];
  char *entry = new char[field_meta.len()];
  memcpy(entry, record + field_meta.offset(), field_meta.len());

  // NULL 检查
  if (field_meta.nullable() && entry[field_meta.len()-1] == '1') {
    delete[] entry;
    return RC::SUCCESS;  // 跳过 NULL 字段
  }

  string text(entry, strnlen(entry, field_meta.len()));
  delete[] entry;

  // 分词
  vector<string> tokens;
  tokenizer_->cut(text, tokens);

  // 更新倒排索引
  doc_lengths_[*rid] = tokens.size();
  for (const auto &token : tokens) {
    inverted_index_[token].postings[*rid]++;
  }

  update_avg_doc_len();
  return RC::SUCCESS;
}
```

#### 分词器（Jieba Tokenizer）

```cpp
class JiebaTokenizer : public Tokenizer {
  cppjieba::Jieba jieba;  // 结巴分词器

  RC cut(const string &text, vector<string> &tokens) override {
    jieba.Cut(text, tokens);
    // 去除停用词（通过访问 KeywordExtractor 的 stopWords_ 私有成员）
    auto &stopWords_ = *GET_PRIVATE(..., &jieba.extractor, stopWords_);
    tokens.erase(remove_if(tokens.begin(), tokens.end(),
      [&](const string &w) { return stopWords_.count(w); }), tokens.end());
    return RC::SUCCESS;
  }
};
```

**注意**：访问私有成员 `stopWords_` 使用了 `private_accessor` 黑魔法（模板元编程），这是 cppjieba 库没有暴露停用词 API 的绕过方案。

#### 取巧之处与问题

**实际问题**：全文索引的搜索是通过 `FunctionExpr::MATCH_AGAINST` 在每次 `get_value()` 调用时执行的。这意味着：

1. **每次执行 TableScan 的 `next()` 时，都会调用一次 `ft_index->search()`**
2. `search()` 会遍历整个倒排索引计算 BM25 分数
3. 对于 WHERE 中有 MATCH_AGAINST 的查询，每行都会触发一次完整的 search，性能极差

**正确做法**：应该创建一个专门的 `FullTextScanPhysicalOperator`：
1. 在 `open()` 时执行一次 `ft_index->search(query)`，获取所有匹配的 `(RID, score)` 列表
2. 在 `next()` 时按 RID 从表中读取记录
3. 将 score 作为额外字段附加到 tuple 上

当前的实现方式（每行都调用 search）在功能上是正确的（因为 search 结果相同，只是浪费了计算），但性能问题明显。

---

## 9. 队友工作分析

### 9.1 UPDATE 语句

#### 实现要点

```sql
UPDATE table_name SET col1=val1, col2=val2 WHERE condition;
```

关键设计：
- **多列更新**：`UPDATE` 支持同时更新多列（`update_list` 语法）
- **UpdateField**：每个更新项存储字段名和表达式

```cpp
struct UpdateField {
  string      attribute_name;  // 字段名
  Expression *expr;            // 新值表达式（支持任意表达式）
};
```

**执行流程**：
1. `UpdatePhysicalOperator::next()` 拉取符合条件的记录
2. 对每条记录，计算各 UpdateField 的 expr 得到新值
3. 构造 new_record（复制 old_record 并修改目标字段）
4. `table->update_record(old, new)`：同时更新索引和记录

**MVCC 下的 UPDATE**：如第 7.3 节所述，MVCC 的 UPDATE 本质上是 DELETE + INSERT。

#### 多行 UPDATE 与唯一约束

如 `UPDATE t SET id = 3 - id`（将 1→2, 2→1），如果逐行更新会触发唯一冲突。正确做法：先收集所有 (old_record, new_value) 对，批量删除旧索引，再批量插入新索引。

---

### 9.2 DATE 类型

#### 存储方式

DATE 在内部存储为 4 字节整数（`int32_t`），格式为 `YYYYMMDD`：

```cpp
void Value::set_date(const char *s) {
  int year, month, day;
  sscanf(s, "%d-%d-%d", &year, &month, &day);
  value_.int_value_ = year * 10000 + month * 100 + day;
  length_ = sizeof(int);
  attr_type_ = AttrType::DATES;
}
```

**例如**：`2024-12-25` → `20241225`

#### 日期合法性检查

```cpp
bool Value::is_valid_date() const {
  int date = get_int();
  int year = date / 10000;
  int month = (date / 100) % 100;
  int day = date % 100;

  if (year < 1 || year > 9999) return false;
  if (month < 1 || month > 12) return false;
  if (month == 2) {
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (day > (leap ? 29 : 28)) return false;
  }
  // ... 其他月份检查
}
```

合法性检查在**词法分析阶段**就进行（`yacc_sql.y` 中），如果日期不合法则将 Value 重置（触发后续的 FAILURE）。

#### DATE_FORMAT 函数

支持 `DATE_FORMAT(date_field, '%Y-%m-%d')` 格式化输出，在 `FunctionExpr` 中实现。

---

### 9.3 表达式（Expression）

#### 支持的表达式类型

```sql
SELECT a + b * 2, LENGTH(name), ROUND(price, 2), DATE_FORMAT(created_at, '%Y-%m')
FROM t
WHERE a > 5 AND (b IS NOT NULL OR c LIKE '%test%')
```

**表达式树示例**（`WHERE a > 5 AND b IS NOT NULL`）：

```
ConjunctionExpr(AND)
├── ComparisonExpr(>)
│   ├── FieldExpr(a)
│   └── ValueExpr(5)
└── ComparisonExpr(IS_NOT_OP)
    ├── FieldExpr(b)
    └── SpecialPlaceholderExpr(NULL)
```

#### ArithmeticExpr

```cpp
RC ArithmeticExpr::get_value(const Tuple &tuple, Value &value) {
  Value left, right;
  left_->get_value(tuple, left);
  right_->get_value(tuple, right);

  switch (arithmetic_type_) {
    case ADD: return Value::add(left, right, value);
    case SUB: return Value::subtract(left, right, value);
    case MUL: return Value::multiply(left, right, value);
    case DIV: return Value::divide(left, right, value);
    case NEGATIVE: value = right; value.negate(); break;
  }
}
```

---

### 9.4 GROUP BY 聚合

#### 实现方式

`HashGroupByPhysicalOperator`：
1. 使用 HashMap 以 group key 为键，聚合状态为值
2. `open()` 阶段遍历所有输入行，填充 HashMap
3. `next()` 阶段逐个返回 HashMap 的条目

**HAVING 子句**：在 GROUP BY 之后，对分组结果进行过滤：

```
HashGroupBy (accumulate all rows by group key)
    └── Predicate (WHERE conditions)
            └── TableScan

然后 HAVING 作为对 GroupBy 输出的额外 Predicate
```

#### 聚合函数

```cpp
enum class AggregateType {
  COUNT, SUM, MAX, MIN, AVG,
  COUNT_STAR,  // COUNT(*)
};
```

`AggregateExpr::get_value()` 不直接计算，而是通过 `Aggregator` 对象累积状态：

```cpp
class SumAggregator : public Aggregator {
  Value sum_;
  RC accumulate(const Value &value) { /* sum_ += value */ }
  RC evaluate(Value &result) { result = sum_; }
};
```

---

### 9.5 别名（Alias）

表别名（`FROM t AS t1`）和列别名（`SELECT a AS col1`）：

**表别名**：在 `RelationNode` 中存储 `ralation_alias`，在 resolve 阶段建立 alias → Table 的映射：

```cpp
struct RelationNode {
  string relation_name;
  string ralation_alias;
};
```

**列别名**：Expression 上有 `alias_` 字段，在生成 TupleSchema 时用 alias 替换列名。

---

### 9.6 ORDER BY 排序

#### 实现

`OrderByPhysicalOperator`：
1. `open()` 阶段**全量读取**所有行并缓存（内存排序）
2. 对缓存行按 order_by 表达式排序（`std::sort`）
3. `next()` 顺序返回排序结果

**排序比较器**：

```cpp
bool compare_tuples(const TupleData &a, const TupleData &b) {
  for (auto &order : order_by_exprs_) {
    Value va, vb;
    order.expr->get_value(a, va);
    order.expr->get_value(b, vb);
    int cmp = va.compare(vb);
    if (cmp != 0) {
      return order.is_asc ? (cmp < 0) : (cmp > 0);
    }
  }
  return false;
}
```

**大数据量问题**：当数据量超过内存限制时（2025 年新增约束），需要外部排序（External Merge Sort），但基础实现只做内存排序。

---

### 9.7 向量类型（VECTOR）

#### 语法支持

```sql
CREATE TABLE t (embedding VECTOR(128));
INSERT INTO t VALUES ([1.0, 2.0, ..., 128.0]);
SELECT L2_DISTANCE(embedding, [1.0, 0.0, ...]) FROM t ORDER BY ... LIMIT 10;
```

**向量距离函数**：
- `L2_DISTANCE(v1, v2)` → 欧氏距离（Euclidean）
- `COSINE_DISTANCE(v1, v2)` → 余弦距离
- `INNER_PRODUCT_DISTANCE(v1, v2)` → 内积距离（负数）

#### 存储

VECTOR 在记录中存储为连续 `float[]`，长度 = 维度 × 4 字节：

```cpp
// 128 维向量 = 128 * sizeof(float) = 512 字节
$$->length = sizeof(float) * $4;  // attr_def 中的长度计算
```

**Value 中的向量**：

```cpp
void Value::set_vector(float *array, int length) {
  attr_type_ = AttrType::VECTORS;
  length_    = length;  // 字节数
  value_.vector_value_ = new float[length / sizeof(float)];
  memcpy(value_.vector_value_, array, length);
  own_data_ = true;
}
```

---

### 9.8 ALTER TABLE

#### 支持的操作

```sql
ALTER TABLE t ADD COLUMN new_col INT;
ALTER TABLE t DROP COLUMN old_col;
ALTER TABLE t CHANGE COLUMN old_name new_name INT;
ALTER TABLE t RENAME TO new_table;
ALTER TABLE t ADD FULLTEXT INDEX idx_name(col) WITH PARSER jieba;
```

#### 实现方式

每种 ALTER 操作的实现（以 ADD COLUMN 为例）：

```cpp
RC HeapTableEngine::add_column(const AttrInfoSqlNode &attr_info, Trx *trx) {
  // 1. 扫描所有现有记录
  RecordScanner *scanner = ...;
  vector<Record> new_records;
  while (scanner->next(record) == RC::SUCCESS) {
    // 2. 为每条记录扩展长度，新字段初始化为 NULL
    char *buf = malloc(table_meta_->record_size() + attr_info.length);
    memcpy(buf, record.data(), table_meta_->record_size());
    buf[table_meta_->record_size() + attr_info.length - 1] = '1';  // NULL 标志
    // 3. 删除旧记录
    delete_record(record);
    new_records.push_back(new_record_with(buf));
  }
  // 4. 更新表元数据（添加字段）
  table_meta_->add_field(attr_info);
  // 5. 修改页头（record_size 变了）
  record_handler_->modify_pages_header(modified_pages, ...);
  // 6. 插入新记录
  for (auto &r : new_records) insert_record(r);
  // 7. 持久化元数据
  flush_table_meta();
}
```

**重命名表**：实际上调用 `db_->rename_table()`，在 Db 中更新 `tables_` map 并重命名文件。

---

### 9.9 视图（VIEW）

#### 基本思路

视图本质上是存储的 SELECT 查询，查询时展开为子查询执行。

```sql
CREATE VIEW v AS SELECT a, b FROM t WHERE a > 0;
SELECT * FROM v WHERE b < 10;
-- 等价于：
SELECT * FROM (SELECT a, b FROM t WHERE a > 0) AS v WHERE b < 10;
```

#### 实现策略（参考经验贴）

利用负数 table_id 区分视图和物理表：

```cpp
// 视图的 table_id 为负数
Table(const string &name, vector<FieldMeta> &&field_metas, int view_id)
    : table_meta_(name, move(field_metas), view_id) {}

bool Table::is_view() const { return table_id() < 0; }
```

**可更新视图**：根据 2025 年赛题规范，单表视图支持 INSERT/UPDATE/DELETE，多表视图和含聚合的视图为只读。

---

### 9.10 UNION 联合查询

#### 实现

```sql
SELECT a FROM t1
UNION ALL
SELECT a FROM t2
UNION
SELECT a FROM t3;
```

**语法解析**：产生 `UnionSqlNode`，包含多个 `UnionUnit`（每个含一个 SelectSqlNode 和 union_type）。

**UnionPhysicalOperator 执行**：

```cpp
// open() 阶段：执行所有子 SELECT，收集结果
for (int i = 0; i < children_.size(); i++) {
  children_[i]->open(trx);
  while (children_[i]->next() == RC::SUCCESS) {
    auto *tuple = children_[i]->current_tuple();
    result_tuples_.push_back(collect_values(tuple));

    if (union_types_[i] == 1 /* UNION */) {
      // 需要去重
      remove_duplicates();
    }
  }
}
current_index_ = 0;

// next() 阶段：顺序返回结果
RC next() {
  if (current_index_ >= result_tuples_.size()) return RC::RECORD_EOF;
  current_tuple_data_ = result_tuples_[current_index_++];
  return RC::SUCCESS;
}
```

**去重实现**：使用 `unordered_set<TupleData, TupleDataHash>` 哈希去重，hash 基于每个值的字符串表示的 XOR。

---

### 9.11 JOIN 连接查询

#### 支持的语法

```sql
-- 显式 INNER JOIN
SELECT * FROM t1 INNER JOIN t2 ON t1.id = t2.id;

-- 隐式内连接（等价）
SELECT * FROM t1, t2 WHERE t1.id = t2.id;
```

**实现策略**：两种语法都被解析并统一处理（见 `yacc_sql.y` 中 join_clauses 的处理）：

```cpp
// 所有 JOIN 的 relation 和 condition 合并进 selection 中
for (auto &join : *$5) {
  $$->selection.relations.emplace_back(join.relation);
  for (auto &cond : join.conditions)
    $$->selection.conditions.emplace_back(move(cond));
}
```

#### NestedLoopJoinPhysicalOperator

```
对外层表的每一行：
  对内层表的每一行：
    评估 JOIN 条件
    如果满足则输出 JoinedTuple
```

时间复杂度：O(N × M)，适合小表或有索引的场景。

---

## 10. 关键设计难点与问题

### 10.1 NULL 值处理的统一性

**问题**：NULL 在不同场景下有不同语义，需要统一设计：

| 场景 | NULL 的行为 |
|------|------------|
| 比较表达式 | `NULL = NULL → NULL`（三值逻辑），不等于任何值 |
| IS NULL / IS NOT NULL | 专门的比较操作符 |
| 唯一索引 | 多个 NULL 不冲突，均可插入 |
| 排序 | NULL 通常视为最小值 |
| 聚合 COUNT | `COUNT(col)` 忽略 NULL，`COUNT(*)` 不忽略 |

**实现挑战**：原始 MiniOB 没有 NULL 支持，需要在多个层面添加：
1. Value 类添加 `is_null_` 字段和 `NULLS` 类型
2. FieldMeta 添加 `nullable_` 字段
3. 记录格式中为可空字段预留标志位（末尾 1 字节）
4. 索引 key 比较时处理 NULL（KeyComparator）
5. ComparisonExpr 中处理 NULL 参与的比较

### 10.2 记录格式与索引格式的协调

**问题**：表记录和索引 key 的内存布局需要严格对应。

- 记录中字段的 `offset` 和 `len` 由 `FieldMeta` 决定
- 索引 key 中字段的 `offset` 由 `IndexMeta::fields_offset_` 决定
- 两者**不一定相同**（记录有系统字段偏移，索引 key 只含索引列）

建立索引时：`BplusTreeIndex::insert_entry(record.data(), rid)` 需要根据 `IndexMeta::fields_` 中的 `offset` 从记录中提取各列数据。

### 10.3 表元数据的持久化

**问题**：每次 CREATE INDEX、ALTER TABLE 后，`TableMeta` 变了，需要同步到磁盘。

**解决方案**：
1. 创建临时文件 `table_name.table.tmp`，写入新的序列化后的元数据
2. 用 `rename()` 原子地替换原文件

```cpp
// 原子性替换元数据文件
string tmp_file = table_meta_file(db_path, name) + ".tmp";
// 写入 tmp_file ...
rename(tmp_file.c_str(), meta_file.c_str());
```

### 10.4 Buffer Size 限制

**问题**：客户端/服务端通信 buffer 太小（8KB），无法传输大 TEXT 数据。

**原因**：`SOCKET_BUFFER_SIZE = 8192`，TEXT 数据可能达到 65535 字节。

**解决**：将 `SOCKET_BUFFER_SIZE` 增大到 `8192 * 1024`（8MB）。

### 10.5 全文索引加载问题

**问题**：全文索引是**内存索引**，不持久化。重启后索引丢失。

**影响**：每次重启 observer 后，全文索引都是空的，查询结果为空。

**解决思路**：重启后扫描表数据，重建全文索引（实际竞赛中可能需要在 open() 时重建）。

### 10.6 MVCC 与唯一索引的交互

**问题**：在 MVCC 模式下，一条被当前事务标记删除（`end_xid = -trx_id`）的记录，其索引项还没有真正删除。此时如果另一个事务插入相同的 key，唯一性检查会误判为冲突。

**解决**：唯一性检查时，需要通过 MVCC 的可见性规则过滤掉不可见的索引项，只有可见的 key 才参与唯一性比较。

---

## 11. 面试常见问题与答案

### Q1：请介绍 MiniOB 项目的整体架构

**答**：MiniOB 是一个完整的关系型数据库，采用经典的分层架构：

- **网络层**：基于 libevent，支持 TCP 和 Unix Socket，使用文本协议或 MySQL 协议
- **SQL 处理层**：词法分析（Flex）→ 语法分析（Bison）→ 语义解析（Stmt）→ 查询优化（逻辑计划+物理计划）→ 火山模型执行
- **存储层**：Buffer Pool 管理页面，RecordManager 管理记录，B+ Tree 实现索引
- **事务层**：MVCC 实现读写并发，CLog 实现 Redo 日志

整个执行链路：SQL 字符串 → ParsedSqlNode → Stmt → 逻辑算子树 → 物理算子树 → 结果集。

---

### Q2：MVCC 是如何实现的？有什么缺陷？

**答**：MiniOB 的 MVCC 通过在每条记录前面附加两个系统字段（`begin_xid` 和 `end_xid`）来实现版本控制：

- 插入时：`begin_xid = -trx_id`（负数表示未提交），`end_xid = INT_MAX`
- 删除时：将 `end_xid` 设为 `-trx_id`
- 提交时：将所有修改记录的 begin/end_xid 从 `-trx_id` 改为 `commit_xid`

可见性判断：事务 T 只能看到 `begin_xid ≤ T.trx_id ≤ end_xid` 的记录。

**主要缺陷**：
1. 提交不是原子的——逐行更新版本号，可能导致部分可见
2. 写写冲突直接报错，不支持等待机制
3. 没有 GC 机制，旧版本记录积累会影响性能

---

### Q3：B+ Tree 索引是如何支持多列的？

**答**：多列索引的 key 是各列数据按顺序拼接而成的字节串，再追加 RID 保证唯一性。`IndexMeta` 维护每列在 key 中的偏移 `fields_offset_`。

`KeyComparator` 按字段顺序逐个比较：先比第一列，相等则比第二列，以此类推。NULL 值（通过末尾标志位识别）被视为最小值，排在最前面。

建立索引时，通过遍历全表记录，从每条记录中按字段偏移提取各列值，拼接成 key 后插入 B+ Tree。

---

### Q4：唯一索引中 NULL 如何处理？

**答**：按照 SQL 标准，唯一索引中允许多个 NULL 值并存（因为 `NULL ≠ NULL`）。

实现上，在执行唯一性检查前，先扫描要插入的 key 中是否有字段为 NULL（通过末尾标志位判断）。如果有任何字段为 NULL，跳过唯一性检查，直接插入；否则才进行 B+ Tree 范围查找检查是否已存在相同 key。

这个细节比较反直觉，调试时花了不少时间才发现。

---

### Q5：全文索引的 BM25 算法是什么？

**答**：BM25（Best Match 25）是信息检索领域最常用的排名函数：

```
BM25(q, d) = Σ_t∈q IDF(t) × [TF(t,d) × (K1+1)] / [TF(t,d) + K1×(1-B+B×|d|/avgdl)]
```

- `IDF(t) = log((N-df+0.5)/(df+0.5))`：词 t 的逆文档频率，df 越大的词权重越低
- `TF(t,d)`：词 t 在文档 d 中的频次
- `K1=1.5`：控制词频饱和，防止高频词权重无限增加
- `B=0.75`：文档长度归一化系数，较短文档对高词频的奖励更大

MiniOB 中用 Jieba 分词后建立倒排索引，每次 MATCH...AGAINST 查询时计算 BM25 分数，按分数降序返回匹配文档。

---

### Q6：描述一下 SQL 完整执行流程（以 SELECT 为例）

**答**：
1. **词法/语法分析**（lex_sql.l + yacc_sql.y）：SQL 字符串 → `ParsedSqlNode`，字段名/表名都是原始字符串
2. **语义解析**（ResolveStage → SelectStmt::create()）：
   - 验证表是否存在，字段是否存在
   - 字符串引用 → 实际 Table*/FieldMeta* 指针
   - 构建 FilterStmt（WHERE 条件的 Expression 树）
3. **逻辑计划生成**（LogicalPlanGenerator）：
   - TableGetLogicalOperator → PredicateLogicalOperator → ProjectLogicalOperator
4. **规则重写**（Rewriter）：谓词下推、常量折叠等
5. **物理计划生成**（PhysicalPlanGenerator）：
   - 选择 TableScan 还是 IndexScan
   - 选择 NestedLoop 还是 HashJoin
6. **执行**（火山模型）：
   - root_operator.open(trx)
   - while (root_operator.next() != RECORD_EOF) → 发送结果行到客户端
   - root_operator.close()

---

### Q7：HeapTable 插入记录时，如果索引唯一约束冲突，如何回滚？

**答**：`HeapTableEngine::insert_record()` 采用了原子化的错误处理：

```cpp
RC insert_record(Record &record) {
  // 1. 先插入堆文件，获得 RID
  rc = record_handler_->insert_record(record.data(), size, &record.rid());

  // 2. 向所有索引插入
  rc = insert_entry_of_indexes(record.data(), record.rid());

  if (rc != RC::SUCCESS) {  // 如果某个索引插入失败（如唯一约束冲突）
    // 3. 回滚索引（删除已成功插入的索引项）
    delete_entry_of_indexes(record.data(), record.rid(), false);
    // 4. 回滚记录
    record_handler_->delete_record(&record.rid());
  }
  return rc;
}
```

这保证了"要么全部成功，要么全部回滚"的原子性。

---

### Q8：你们的全文索引实现有什么问题？正确实现是什么？

**答**：当前实现中，`MATCH(col) AGAINST('query')` 是作为普通函数表达式处理的，在每次 TableScan 的 next() 时，`FunctionExpr::get_value()` 都会调用一次 `FullTextIndex::search()`，将整个查询文本在倒排索引中搜索一遍，非常浪费。

**正确实现**应该创建专门的 `FullTextScanPhysicalOperator`：
1. 在物理计划生成阶段，识别出 WHERE 子句中包含 MATCH_AGAINST 的情况
2. 将 TableScan + MATCH_AGAINST 整合为 FullTextScanOperator
3. 在 `open()` 时执行一次 `search()`，得到所有 `(RID, score)` 结果
4. 在 `next()` 时按 RID 顺序读取记录，并将 score 附加到 tuple

这样 search() 只执行一次，O(1) 时间内返回结果列表。

---

### Q9：缓冲池是如何工作的？

**答**：Buffer Pool 使用固定大小（8KB）的内存帧（Frame）缓存磁盘页面：

- **映射关系**：`(file_id, page_num)` → `Frame*`（哈希表）
- **替换策略**：LRU（最近最少使用），当帧满时淘汰最久未使用的帧
- **Pin 机制**：正在使用的帧不能被淘汰（pin_count > 0）
- **脏页**：修改后需标记为 dirty，刷盘时才真正写入磁盘

**写入流程**：
1. `fetch_page(page_num)` → 如果不在内存则从磁盘读取，增加 pin_count
2. 修改 Frame 的数据
3. 标记 Frame 为 dirty
4. `unpin_page(frame)` → 减少 pin_count，允许被淘汰
5. 后台/显式调用 `flush_page()` 写回磁盘

---

### Q10：如何添加一个新的 SQL 特性？（以 DROP INDEX 为例）

**答**：

1. **词法/语法**（`lex_sql.l` / `yacc_sql.y`）：确保 `DROP INDEX` 关键字已有，添加语法规则生成 `DropIndexSqlNode`

2. **parse_defs.h**：添加 `DropIndexSqlNode` 结构和 `SCF_DROP_INDEX` 枚举值

3. **Stmt**（`drop_index_stmt.h/cpp`）：实现 `DropIndexStmt::create_stmt()`，验证索引存在

4. **Executor**（`drop_index_executor.h/cpp`）：实现 `execute()` 方法，调用 `table->drop_index(index_name)`

5. **CommandExecutor**（`command_executor.cpp`）：在 switch-case 中添加 `SCF_DROP_INDEX` 分支

6. **Table** / **HeapTableEngine**：实现 `drop_index()` 方法：关闭索引、删除文件、更新元数据

这个模式在 MiniOB 中高度标准化，每个 DDL 特性都遵循相同的添加流程。

---

## 12. 重要知识点总结

### 12.1 数据库基础知识

#### ACID 属性
- **原子性（Atomicity）**: 事务中的操作要么全部成功，要么全部回滚
- **一致性（Consistency）**: 事务执行前后，数据库处于合法状态
- **隔离性（Isolation）**: 并发事务互不干扰（MVCC 提供快照隔离）
- **持久性（Durability）**: 已提交事务的结果永久保存（Redo Log 保证）

#### 隔离级别
| 级别 | 脏读 | 不可重复读 | 幻读 |
|------|------|-----------|------|
| READ UNCOMMITTED | ✓ | ✓ | ✓ |
| READ COMMITTED | ✗ | ✓ | ✓ |
| REPEATABLE READ | ✗ | ✗ | ✓ |
| SERIALIZABLE | ✗ | ✗ | ✗ |

MiniOB MVCC 近似实现了**快照隔离（Snapshot Isolation）**，介于 READ COMMITTED 和 REPEATABLE READ 之间。

### 12.2 B+ Tree 关键性质

1. **所有数据在叶子节点**：内部节点只存键，不存数据（非聚簇索引）
2. **叶子节点链表**：支持高效的范围扫描
3. **平衡树**：所有叶子到根的路径长度相同
4. **节点利用率**：节点至少半满（合并/重分配保证）
5. **索引高度**：N 条记录，每节点最多 M 个 key，树高约 log_M(N)

**B+ Tree vs B Tree**：
- B+ Tree 的叶子节点存数据，B Tree 的每个节点都存数据
- B+ Tree 的叶子节点通过链表连接，范围查询效率更高
- 数据库索引几乎都用 B+ Tree

### 12.3 缓冲池设计

**关键技术**：
- **LRU-K 替换策略**：比纯 LRU 更智能，防止大扫描污染缓存
- **预读（Read-Ahead）**：顺序扫描时预先加载后续页面
- **双写缓冲（Double Write Buffer）**：防止部分写入导致页面损坏
- **WAL（Write-Ahead Logging）**：日志先于数据页写入磁盘

### 12.4 日志与恢复

**WAL 原则**：
1. 修改数据页前，先写 Redo Log
2. 提交事务前，确保所有 Log 都已持久化

**检查点（Checkpoint）**：
- 定期将内存脏页刷盘，减少恢复时需要重放的日志量
- 记录检查点时刻所有活跃事务的状态

**崩溃恢复过程（ARIES 算法）**：
1. **分析阶段**：扫描日志，确定哪些事务已提交，哪些未提交
2. **重做阶段（Redo）**：重放所有日志（包括未提交事务）
3. **撤销阶段（Undo）**：回滚所有未提交事务

### 12.5 火山模型 vs 向量化执行

| 特性 | 火山模型 (Volcano) | 向量化执行 |
|------|-------------------|----------|
| 处理粒度 | 一次一行 | 一次一批（Chunk） |
| 函数调用开销 | 高（每行都需要） | 低（每批调用一次） |
| CPU 缓存利用 | 较差 | 较好（列数据连续） |
| 适合场景 | OLTP | OLAP |
| MiniOB 支持 | ✓（默认） | ✓（向量化算子） |

### 12.6 全文搜索关键概念

- **倒排索引（Inverted Index）**: `词 → [文档ID, 词频]` 的映射，是搜索引擎的核心数据结构
- **TF-IDF**: 词频-逆文档频率，衡量词对文档的重要性
- **BM25**: TF-IDF 的改进版，考虑了文档长度归一化和词频饱和
- **分词（Tokenization）**: 将文本切分为词的过程，中文需要专门的分词算法（如结巴）
- **停用词（Stop Words）**: 如"的"、"了"、"在"等高频但无意义的词，通常从索引中去除

### 12.7 索引设计原则

**什么时候用索引**：
- 高选择性列（`WHERE col = val` 能过滤大量数据）
- 范围查询列（`WHERE col BETWEEN a AND b`）
- JOIN 连接列（`ON t1.id = t2.id`）
- ORDER BY 列（可以利用索引有序性避免排序）

**索引的代价**：
- 占用磁盘空间
- 写操作需要维护索引（INSERT/UPDATE/DELETE 变慢）
- 过多索引可能让优化器迷惑

**多列索引的最左前缀原则**：
对于索引 `(a, b, c)`：
- `WHERE a = 1` → 可用
- `WHERE a = 1 AND b = 2` → 可用
- `WHERE b = 2` → **不可用**（没有最左列 a）
- `WHERE a = 1 AND c = 3` → 只能用到 a 的部分

### 12.8 SQL 优化技术

**谓词下推（Predicate Pushdown）**：将 WHERE 条件尽早执行（靠近叶节点），减少中间结果集大小。

```sql
-- 优化前
SELECT * FROM (SELECT * FROM t1 JOIN t2) AS tmp WHERE tmp.id > 5;

-- 优化后（等价）
SELECT * FROM t1 JOIN t2 WHERE t1.id > 5 OR t2.id > 5;
```

**常量折叠（Constant Folding）**：

```sql
WHERE 1 + 1 = 2  →  WHERE TRUE（编译期计算）
WHERE FALSE AND col = 5  →  FALSE（短路求值）
```

**索引选择（Index Selection）**：
- 统计信息（选择率）决定是否走索引
- 全表扫描 vs 索引扫描的 I/O 代价比较

### 12.9 MiniOB 与 MySQL 的对比

| 特性 | MiniOB | MySQL |
|------|--------|-------|
| 存储引擎 | 单一堆表引擎 | InnoDB/MyISAM/... |
| 并发控制 | 简单 MVCC | 完整 MVCC + 锁 |
| 日志 | 简化 Redo Log | Redo + Undo Log |
| 索引类型 | B+ Tree + FullText | B+ Tree + Full-Text + Hash + ... |
| 集群索引 | 不支持 | InnoDB 支持 |
| 分区 | 不支持 | 支持 |
| 复制 | 不支持 | 支持 |

### 12.10 Flex/Bison 工具链

**Flex（词法分析器生成器）**：
- 输入：正则表达式规则文件（.l）
- 输出：C 代码，识别 token
- MiniOB 中：`lex_sql.l` → `lex_sql.cpp`

**Bison（语法分析器生成器，基于 LALR(1) 文法）**：
- 输入：BNF 语法规则文件（.y）
- 输出：C 代码，解析 token 流为 AST
- MiniOB 中：`yacc_sql.y` → `yacc_sql.cpp`

**添加新语法的步骤**：
1. 在 `.l` 中添加 token 的词法规则（关键字识别）
2. 在 `.y` 的 `%token` 部分声明新 token
3. 在 `%union` 部分添加对应的数据类型（如有需要）
4. 添加语法规则（BNF 产生式）和对应的 `{ C++ action 代码 }`
5. 重新运行 flex/bison 生成代码（CMake 自动处理）

---

## 附录：文件结构速查

```
src/observer/
├── sql/
│   ├── parser/
│   │   ├── lex_sql.l          ← 词法规则
│   │   ├── yacc_sql.y         ← 语法规则
│   │   └── parse_defs.h       ← AST 节点定义
│   ├── stmt/                   ← Stmt 类（语义解析）
│   │   ├── stmt.h
│   │   ├── select_stmt.h/cpp
│   │   ├── create_index_stmt.h/cpp
│   │   └── ...
│   ├── optimizer/             ← 查询优化
│   │   ├── logical_plan_generator.h/cpp
│   │   ├── physical_plan_generator.h/cpp
│   │   └── rewriter.h/cpp
│   ├── executor/              ← DDL 执行器
│   │   ├── execute_stage.h/cpp
│   │   ├── command_executor.h/cpp
│   │   ├── create_index_executor.h/cpp
│   │   └── ...
│   ├── operator/              ← DML 物理算子
│   │   ├── physical_operator.h
│   │   ├── table_scan_physical_operator.h/cpp
│   │   ├── index_scan_physical_operator.h/cpp
│   │   ├── predicate_physical_operator.h/cpp
│   │   └── ...
│   └── expr/                  ← 表达式
│       ├── expression.h/cpp
│       └── tuple.h/cpp
├── storage/
│   ├── table/
│   │   ├── table.h/cpp         ← 表接口
│   │   ├── table_meta.h/cpp    ← 表元数据
│   │   ├── heap_table_engine.h/cpp ← 堆表引擎
│   │   └── ...
│   ├── record/
│   │   ├── record.h            ← Record/RID
│   │   └── record_manager.h/cpp ← 记录管理
│   ├── index/
│   │   ├── index.h             ← 索引接口
│   │   ├── index_meta.h/cpp    ← 索引元数据
│   │   ├── bplus_tree.h/cpp    ← B+ Tree 实现
│   │   ├── bplus_tree_index.h/cpp ← B+ Tree 索引
│   │   └── full_text_index.h/cpp  ← 全文索引
│   ├── buffer/
│   │   └── disk_buffer_pool.h/cpp ← Buffer Pool
│   ├── trx/
│   │   ├── trx.h               ← 事务接口
│   │   └── mvcc_trx.h/cpp      ← MVCC 实现
│   ├── tokenizer/
│   │   └── jieba_tokenizer.h/cpp ← 结巴分词器
│   └── clog/                   ← Redo 日志
├── common/
│   ├── value.h/cpp             ← Value 类
│   ├── type/                   ← 数据类型系统
│   │   ├── data_type.h/cpp
│   │   ├── char_type.h/cpp
│   │   ├── integer_type.h/cpp
│   │   └── ...
│   └── ini_setting.h           ← 配置常量（含 SOCKET_BUFFER_SIZE）
└── net/                        ← 网络层
    └── ...
```

---

*本文档涵盖了 MiniOB 项目的核心技术点、实现细节和面试准备内容。建议结合代码阅读，深入理解每个模块的设计决策。*

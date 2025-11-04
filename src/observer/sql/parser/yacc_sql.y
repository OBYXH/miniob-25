
%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/log/log.h"
#include "common/lang/string.h"
#include "sql/parser/parse_defs.h"
#include "sql/parser/yacc_sql.hpp"
#include "sql/parser/lex_sql.h"
#include "sql/expr/expression.h"
#include <memory>

using namespace std;

char* unescape_sql_string(const char* sql_string) {
    if (sql_string == nullptr) return nullptr;
    size_t len = strlen(sql_string);
    // 至少需要两个引号 ''
    if (len < 2) return strdup(sql_string);

    // 分配足够的内存 (长度肯定不会超过原长)
    char* result = (char*)malloc(len); 
    if (result == nullptr) return nullptr;

    size_t j = 0; // result string index
    // 遍历引号内的内容
    for (size_t i = 1; i < len - 1; ++i) {
        // 如果发现连续两个单引号 ''
        if (sql_string[i] == '\'' && i + 1 < len - 1 && sql_string[i+1] == '\'') {
            result[j++] = '\''; // 只写入一个 '
            i++; // 并且跳过下一个 '
        } else {
            result[j++] = sql_string[i]; // 复制其他所有字符
        }
    }
    result[j] = '\0'; // 添加字符串结束符
    return result;
}

string token_name(const char *sql_string, YYLTYPE *llocp)
{
  return string(sql_string + llocp->first_column, llocp->last_column - llocp->first_column + 1);
}

int yyerror(YYLTYPE *llocp, const char *sql_string, ParsedSqlResult *sql_result, yyscan_t scanner, const char *msg)
{
  unique_ptr<ParsedSqlNode> error_sql_node = make_unique<ParsedSqlNode>(SCF_ERROR);
  error_sql_node->error.error_msg = msg;
  error_sql_node->error.line = llocp->first_line;
  error_sql_node->error.column = llocp->first_column;
  sql_result->add_sql_node(std::move(error_sql_node));
  return 0;
}

ArithmeticExpr *create_arithmetic_expression(ArithmeticExpr::Type type,
                                             Expression *left,
                                             Expression *right,
                                             const char *sql_string,
                                             YYLTYPE *llocp)
{
  ArithmeticExpr *expr = new ArithmeticExpr(type, left, right);
  expr->set_name(token_name(sql_string, llocp));
  return expr;
}


%}

%define api.pure full
%define parse.error verbose
/** 启用位置标识 **/
%locations
%lex-param { yyscan_t scanner }
/** 这些定义了在yyparse函数中的参数 **/
%parse-param { const char * sql_string }
%parse-param { ParsedSqlResult * sql_result }
%parse-param { void * scanner }

//标识tokens
%token  SEMICOLON
        BY
        CREATE
        DROP
        GROUP
        ORDER
        ASC
        TABLE
        TABLES
        INDEX
        CALC
        SELECT
        DESC
        SHOW
        SYNC
        INSERT
        DELETE
        UPDATE
        LBRACE
        RBRACE
        COMMA
        TRX_BEGIN
        TRX_COMMIT
        TRX_ROLLBACK
        INT_T
        STRING_T
        FLOAT_T
        DATE_T
        VECTOR_T
        TRUE
        FALSE
        HELP
        EXIT
        DOT //QUOTE
        INTO
        VALUES
        FROM
        WHERE
        AND
        OR
        SET
        ON
        LOAD
        DATA
        UNIQUE
        INFILE
        EXPLAIN
        STORAGE
        FORMAT
        PRIMARY
        KEY
        ANALYZE
        FIELDS
        TERMINATED
        ENCLOSED
        EQ
        LT
        GT
        LE
        GE
        NE
        NOT
        LIKE
        NULL_T
        NULLABLE
        IS
        IN
        EXISTS
        AS
        HAVING
        TEXT_T
        LIMIT
        ALTER
        ADD
        COLUMN
        CHANGE
        TO
        RENAME
        INNER
        JOIN
        UNION
        ALL
        AGAINST
        PARSER
        WITH
        FULLTEXT
        STRING_TO_VECTOR
        

/** union 中定义各种数据类型，真实生成的代码也是union类型，所以不能有非POD类型的数据 **/
%union {
  ParsedSqlNode *                            sql_node;
  Value *                                    value;
  enum CompOp                                comp;
  RelAttrSqlNode *                           rel_attr;
  vector<AttrInfoSqlNode> *                  attr_infos;
  AttrInfoSqlNode *                          attr_info;
  Expression *                               expression;
  vector<unique_ptr<Expression>> *           expression_list;
  vector<Value> *                            value_list;
  std::vector<std::vector<Value>> *          values_list;
  vector<RelAttrSqlNode> *                   rel_attr_list;
  vector<RelationNode> *                     relation_list;
  JoinSqlNode *                              join_clauses;
  vector<string> *                           key_list;
  OrderBySqlNode *                           orderby_unit;
  std::vector<OrderBySqlNode> *              orderby_list;
  LimitSqlNode *                             limit_node;
  vector<UnionUnit> *                        union_list;
  char *                                     cstring;
  int                                        number;
  float                                      floats;
  bool                                       nullable_info;
  bool                                       unique;
  vector<UpdateField> *                      update_list;
  float                                      digits;
}

%destructor { delete $$; } <value>
%destructor { delete $$; } <rel_attr>
%destructor { delete $$; } <attr_infos>
%destructor { delete $$; } <expression>
%destructor { delete $$; } <expression_list>
%destructor { delete $$; } <value_list>
// %destructor { delete $$; } <rel_attr_list>
%destructor { delete $$; } <relation_list>
%destructor { delete $$; } <key_list>

%token <number> NUMBER
%token <floats> FLOAT
%token <cstring> ID
%token <cstring> SSS
%token <cstring> VECTOR
//非终结符

/** type 定义了各种解析后的结果输出的是什么类型。类型对应了 union 中的定义的成员变量名称 **/
%type <number>              type
%type <value>               value
%type <expression>           condition
%type <number>              number
%type <cstring>             relation
%type <comp>                comp_op
%type <rel_attr>            rel_attr
%type <nullable_info>       nullable_constraint
%type <attr_infos>          attr_def_list
%type <attr_info>           attr_def
%type <value_list>          value_list
%type <values_list>         values_list
%type <expression>      where
%type <expression>      having_condition
%type <cstring>             storage_format
%type <key_list>            primary_key
%type <key_list>            attr_list
%type <relation_list>       rel_list
%type <join_clauses>       join_clauses
%type <expression>          expression
%type <expression>          function_expression
%type <expression_list>     expression_list
%type <expression_list>     group_by
%type <orderby_unit>        sort_unit
%type <orderby_list>        sort_list
%type <orderby_list>        opt_order_by
%type <limit_node>          opt_limit
%type <cstring>             fields_terminated_by
%type <cstring>             enclosed_by
%type <cstring>             alias
%type <unique>              opt_unique
%type <sql_node>            alter_stmt
%type <sql_node>            calc_stmt
%type <sql_node>            select_stmt
%type <sql_node>            union_stmt
%type <sql_node>            insert_stmt
%type <sql_node>            update_stmt
%type <sql_node>            delete_stmt
%type <sql_node>            create_table_stmt
%type <sql_node>            drop_table_stmt
%type <sql_node>            analyze_table_stmt
%type <sql_node>            show_tables_stmt
%type <sql_node>            show_index_stat
%type <sql_node>            desc_table_stmt
%type <sql_node>            create_index_stmt
%type <sql_node>            drop_index_stmt
%type <sql_node>            sync_stmt
%type <sql_node>            begin_stmt
%type <sql_node>            commit_stmt
%type <sql_node>            rollback_stmt
%type <sql_node>            load_data_stmt
%type <sql_node>            explain_stmt
%type <sql_node>            set_variable_stmt
%type <sql_node>            help_stmt
%type <sql_node>            exit_stmt
%type <sql_node>            command_wrapper
// commands should be a list but I use a single command instead
%type <sql_node>            commands
%type <update_list>         update_list
%type <union_list>          union_list

%left '+' '-'
%left '*' '/'
%right UMINUS
%%

commands: command_wrapper opt_semicolon  //commands or sqls. parser starts here.
  {
    unique_ptr<ParsedSqlNode> sql_node = unique_ptr<ParsedSqlNode>($1);
    sql_result->add_sql_node(std::move(sql_node));
  }
  ;

command_wrapper:
    calc_stmt
  | select_stmt
  | union_stmt
  | insert_stmt
  | update_stmt
  | delete_stmt
  | create_table_stmt
  | drop_table_stmt
  | analyze_table_stmt
  | show_tables_stmt
  | show_index_stat
  | desc_table_stmt
  | create_index_stmt
  | drop_index_stmt
  | sync_stmt
  | begin_stmt
  | commit_stmt
  | rollback_stmt
  | load_data_stmt
  | explain_stmt
  | set_variable_stmt
  | help_stmt
  | exit_stmt
  | alter_stmt
    ;

exit_stmt:      
    EXIT {
      (void)yynerrs;  // 这么写为了消除yynerrs未使用的告警。如果你有更好的方法欢迎提PR
      $$ = new ParsedSqlNode(SCF_EXIT);
    };

help_stmt:
    HELP {
      $$ = new ParsedSqlNode(SCF_HELP);
    };

sync_stmt:
    SYNC {
      $$ = new ParsedSqlNode(SCF_SYNC);
    }
    ;

begin_stmt:
    TRX_BEGIN  {
      $$ = new ParsedSqlNode(SCF_BEGIN);
    }
    ;

commit_stmt:
    TRX_COMMIT {
      $$ = new ParsedSqlNode(SCF_COMMIT);
    }
    ;

rollback_stmt:
    TRX_ROLLBACK  {
      $$ = new ParsedSqlNode(SCF_ROLLBACK);
    }
    ;

drop_table_stmt:    /*drop table 语句的语法解析树*/
    DROP TABLE ID {
      $$ = new ParsedSqlNode(SCF_DROP_TABLE);
      $$->drop_table.relation_name = $3;
    };

analyze_table_stmt:  /* analyze table 语法的语法解析树*/
    ANALYZE TABLE ID {
      $$ = new ParsedSqlNode(SCF_ANALYZE_TABLE);
      $$->analyze_table.relation_name = $3;
    }
    ;

show_tables_stmt:
    SHOW TABLES {
      $$ = new ParsedSqlNode(SCF_SHOW_TABLES);
    }
    ;

show_index_stat:
    SHOW INDEX FROM relation {
      $$ = new ParsedSqlNode(SCF_SHOW_INDEX);
      ShowIndexSqlNode &show_index = $$->show_index;
      show_index.relation_name = $4;
    }

desc_table_stmt:
    DESC ID  {
      $$ = new ParsedSqlNode(SCF_DESC_TABLE);
      $$->desc_table.relation_name = $2;
    }
    ;

create_index_stmt:    /*create index 语句的语法解析树*/
    CREATE opt_unique INDEX ID ON ID LBRACE attr_list RBRACE
    {
      $$ = new ParsedSqlNode(SCF_CREATE_INDEX);
      CreateIndexSqlNode &create_index = $$->create_index;
      create_index.unique = $2;
      create_index.index_name = $4;
      create_index.relation_name = $6;
      create_index.attribute_name.swap(*$8);
      delete $8;
    }
    | CREATE VECTOR_T INDEX ID ON ID LBRACE attr_list RBRACE
    {
      $$ = new ParsedSqlNode(SCF_CREATE_INDEX);
      CreateIndexSqlNode &create_index = $$->create_index;
      create_index.unique = false; // 向量索引不支持
      create_index.index_name = $4;
      create_index.relation_name = $6;
      create_index.attribute_name.swap(*$8); // $8 是 vector<string> 类型
      create_index.index_type = IndexType::VectorIVFFlatIndex;
      delete $8; // 释放指针
    }
    ;
  
opt_unique:
    UNIQUE {
      $$ = true;
    } | {
      $$ = false;
    }
    ;

drop_index_stmt:      /*drop index 语句的语法解析树*/
    DROP INDEX ID ON ID
    {
      $$ = new ParsedSqlNode(SCF_DROP_INDEX);
      $$->drop_index.index_name = $3;
      $$->drop_index.relation_name = $5;
    }
    ;
create_table_stmt:    /*create table 语句的语法解析树*/
    CREATE TABLE ID LBRACE attr_def_list primary_key RBRACE storage_format
    {
      $$ = new ParsedSqlNode(SCF_CREATE_TABLE);
      CreateTableSqlNode &create_table = $$->create_table;
      create_table.relation_name = $3;
      //free($3);

      create_table.attr_infos.swap(*$5);
      delete $5;

      if ($6 != nullptr) {
        create_table.primary_keys.swap(*$6);
        delete $6;
      }
      if ($8 != nullptr) {
        create_table.storage_format = $8;
      }
    }
    ;
    
attr_def_list:
    attr_def
    {
      $$ = new vector<AttrInfoSqlNode>;
      $$->emplace_back(*$1);
      delete $1;
    }
    | attr_def_list COMMA attr_def
    {
      $$ = $1;
      $$->emplace_back(*$3);
      delete $3;
    }
    ;
    
attr_def:
    ID type LBRACE number RBRACE nullable_constraint
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      if ($$->type == AttrType::CHARS) {
        $$->length = $4;
      } else if ($$->type == AttrType::VECTORS) {
        $$->length = sizeof(float) * $4;
      } else {
        ASSERT(false, "$$->type is invalid.");
      }
      $$->nullable = $6;
      if ($$->nullable) {
        $$->length++;
      }
    }
    | ID type nullable_constraint
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      if ($$->type == AttrType::INTS) {
        $$->length = sizeof(int);
      } else if ($$->type == AttrType::FLOATS) {
        $$->length = sizeof(float);
      } else if ($$->type == AttrType::DATES) {
        $$->length = sizeof(int);
      } else if ($$->type == AttrType::CHARS) {
        $$->length = sizeof(char) * 4;
      } else if ($$->type == AttrType::VECTORS) {
        $$->length = sizeof(float) * 1;
      } else if ($$->type == AttrType::TEXTS) {
        $$->length = 16384;
      } else {
        ASSERT(false, "$$->type is invalid.");
      }
      $$->nullable = $3;
      if ($$->nullable) {
        $$->length++;
      }
    }
    ;

nullable_constraint:
    NOT NULL_T
    {
      $$ = false;  // NOT NULL 对应的可空性为 false
    }
    | NULLABLE
    {
      $$ = true;  // NULLABLE 对应的可空性为 true 2022
    }
    | NULL_T
    {
      $$ = true;  // NULL 对应的可空性也为 true 2023
    }
    | /* empty */
    {
      $$ = true;  // 默认情况为 NULL
    }
    ;

number:
    NUMBER {$$ = $1;}
    ;
type:
    INT_T      { $$ = static_cast<int>(AttrType::INTS); }
    | STRING_T { $$ = static_cast<int>(AttrType::CHARS); }
    | FLOAT_T  { $$ = static_cast<int>(AttrType::FLOATS); }
    | VECTOR_T { $$ = static_cast<int>(AttrType::VECTORS); }
    | DATE_T   { $$ = static_cast<int>(AttrType::DATES); }
    | TEXT_T   { $$ = static_cast<int>(AttrType::TEXTS); }
    ;
primary_key:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA PRIMARY KEY LBRACE attr_list RBRACE
    {
      $$ = $5;
    }
    ;

attr_list:
    ID {
      $$ = new vector<string>();
      $$->emplace_back($1);
    }
    | ID COMMA attr_list {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new vector<string>;
      }

      $$->emplace($$->begin(), $1);
    }
    ;

alter_stmt:
    ALTER TABLE ID ADD COLUMN attr_def{
      $$ = new ParsedSqlNode(SCF_ALTER);
      AlterSqlNode &alter_table = $$->alter_table;
      alter_table.relation_name = $3;
      alter_table.alter_type = AlterType::ALTER_ADD;
      alter_table.old_attr_info = $6;
    }
    |ALTER TABLE ID DROP COLUMN ID{
      $$ = new ParsedSqlNode(SCF_ALTER);
      AlterSqlNode &alter_table = $$->alter_table;
      alter_table.relation_name = $3;
      alter_table.alter_type = AlterType::ALTER_DROP;
      alter_table.old_attr_info = new AttrInfoSqlNode;
      alter_table.old_attr_info->name = $6;
    }
    | ALTER TABLE ID CHANGE COLUMN ID ID type {
      $$ = new ParsedSqlNode(SCF_ALTER);
      AlterSqlNode &alter_table = $$->alter_table;
      alter_table.relation_name = $3;
      alter_table.alter_type = AlterType::ALTER_CHANGE;
      alter_table.old_attr_info = new AttrInfoSqlNode;
      alter_table.old_attr_info->name = $6;
      alter_table.old_attr_info->type = static_cast<AttrType>($8);
      alter_table.new_attribute_name = $7;
    }
    | ALTER TABLE ID RENAME TO ID {
      $$ = new ParsedSqlNode(SCF_ALTER);
      AlterSqlNode &alter_table = $$->alter_table;
      alter_table.relation_name = $3;
      alter_table.alter_type = AlterType::ALTER_RENAME;
      alter_table.old_attr_info = new AttrInfoSqlNode;
      alter_table.new_relation_name = $6;
    }
    | ALTER TABLE ID ADD FULLTEXT INDEX ID LBRACE ID RBRACE WITH PARSER ID {
      $$ = new ParsedSqlNode(SCF_ALTER);
      AlterSqlNode &alter_table = $$->alter_table;
      alter_table.relation_name = $3;
      alter_table.alter_type = AlterType::ALTER_ADD_FULLTEXT_INDEX;
      alter_table.old_attr_info = new AttrInfoSqlNode;
      alter_table.index_name = $7;
      alter_table.index_column = $9;
      alter_table.parser_name = $13;
    }
    ;

insert_stmt:        /*insert   语句的语法解析树*/
    INSERT INTO ID VALUES values_list
    {
      $$ = new ParsedSqlNode(SCF_INSERT);
      $$->insertion.relation_name = $3;
      if ($5 != nullptr) {
        $$->insertion.values_list.swap(*$5);
        delete $5;
      }
    }
    ;

values_list:
      LBRACE value_list RBRACE
    {
      $$ = new std::vector<std::vector<Value>>;
      $$->emplace_back(std::move(*$2));
      delete $2;
    }
    | values_list COMMA LBRACE value_list RBRACE
    {
      $$->emplace_back(std::move(*$4));
      delete $4;
    }

value_list:
    /* empty */
    {
      $$ = new vector<Value>;
    }
    | value
    {
      $$ = new vector<Value>;
      $$->reserve(3);
      $$->emplace_back(std::move(*$1));
      delete $1;
    }
    | value_list COMMA value
    {
      $$ = $1;
      $$->emplace_back(std::move(*$3));
      delete $3;
    }
    ;

value:
    '-' NUMBER {
      $$ = new Value(-(int)$2);
      @$ = @1;
    }
    | '-' FLOAT {
      $$ = new Value(-(float)$2);
      @$ = @1;
    }
    | NUMBER {
      $$ = new Value((int)$1);
      @$ = @1;
    }
    | FLOAT {
      $$ = new Value((float)$1);
      @$ = @1;
    }
    | SSS {
      char *tmp = unescape_sql_string($1);
      $$ = new Value(tmp);
      free(tmp);
    }
    | TRUE {
      $$ = new Value(true);
    }
    | FALSE {
      $$ = new Value(false);
    }
    | NULL_T {
      $$ = new Value();
      $$->set_null();
    }
    | STRING_TO_VECTOR LBRACE VECTOR RBRACE {
      char *tmp = unescape_sql_string($3);
      Value temp_val = Value::string_to_vector(tmp);
      $$ = new Value(std::move(temp_val));
      free(tmp);
    }
    | VECTOR {
      char *tmp = unescape_sql_string($1);
      if (tmp != nullptr) {
        Value temp_val = Value::string_to_vector(tmp); 
        $$ = new Value(std::move(temp_val));  
        free(tmp);
      } else {
        $$ = new Value();
      }
    }
    ;

storage_format:
    /* empty */
    {
      $$ = nullptr;
    }
    | STORAGE FORMAT EQ ID
    {
      $$ = $4;
    }
    ;
    
delete_stmt:    /*  delete 语句的语法解析树*/
    DELETE FROM ID where 
    {
      $$ = new ParsedSqlNode(SCF_DELETE);
      $$->deletion.relation_name = $3;
      if ($4 != nullptr) {
        $$->deletion.condition = std::unique_ptr<Expression>($4);
      }
    }
    ;
update_stmt:      /*  update 语句的语法解析树*/
    UPDATE ID SET update_list where 
    {
      $$ = new ParsedSqlNode(SCF_UPDATE);
      $$->update.relation_name = $2;
      $$->update.update_list.swap(*$4);
      if ($5 != nullptr) {
        $$->update.conditions = std::unique_ptr<Expression>($5);
      }
      delete $4;
    }
    ;

update_list:
    ID EQ expression{
      $$ = new vector<UpdateField>();
      $$->emplace_back(string($1),unique_ptr<Expression>($3));
    }
    | ID EQ expression COMMA update_list {
      $$ = $5;
      $$->emplace_back(string($1),unique_ptr<Expression>($3));
    }
    ;

union_list:
    UNION ALL select_stmt {
      $$ = new vector<UnionUnit>;
      UnionUnit union_unit;
      union_unit.selection = std::move($3->selection);
      union_unit.union_type = 0;
      $$->emplace_back(std::move(union_unit));
      delete $3;
    }
    | UNION select_stmt {
      $$ = new vector<UnionUnit>;
      UnionUnit union_unit;
      union_unit.selection = std::move($2->selection);
      union_unit.union_type = 1;
      $$->emplace_back(std::move(union_unit));      
      delete $2;
    }
    | UNION ALL select_stmt union_list {
      $$ = $4;
      UnionUnit union_unit;
      union_unit.selection = std::move($3->selection);
      union_unit.union_type = 0;
      $$->emplace_back(std::move(union_unit));
      delete $3;
    }
    | UNION select_stmt union_list {
      $$ = $3;
      UnionUnit union_unit;
      union_unit.selection = std::move($2->selection);
      union_unit.union_type = 1;
      $$->emplace_back(std::move(union_unit)); 
      delete $2;      
    }
    ;
union_stmt:
    select_stmt union_list{
      $$ = new ParsedSqlNode(SCF_UNION);
      UnionUnit union_unit;
      union_unit.selection = std::move($1->selection);
      union_unit.union_type = 0;
      $$->union_node.unions.emplace_back(std::move(union_unit));
      std::reverse($2->begin(), $2->end());
      for (auto &unit : *$2) {
        $$->union_node.unions.emplace_back(std::move(unit));
      }
      delete $1;
      delete $2;
    }
    ;
select_stmt:        /*  select 语句的语法解析树*/
    SELECT expression_list FROM rel_list where group_by having_condition opt_order_by opt_limit
    {
      $$ = new ParsedSqlNode(SCF_SELECT);
      if ($2 != nullptr) {
        $$->selection.expressions.swap(*$2);
        delete $2;
      }

      if ($4 != nullptr) {
        $$->selection.relations.swap(*$4);
        delete $4;
      }

      if ($5 != nullptr) {
        $$->selection.conditions = std::unique_ptr<Expression>($5);
      }

      if ($6 != nullptr) {
        $$->selection.group_by.swap(*$6);
        delete $6;
      }

      if( $7 != nullptr) {
        $$->selection.having_conditions = std::unique_ptr<Expression>($7);
        delete $7;
      }

      if ($8 != nullptr) {
        $$->selection.order_by.swap(*$8);
        delete $8;
      }

      if ($9 != nullptr) {
        $$->selection.limit = std::make_unique<LimitSqlNode>(*$9);
        delete $9;
      }
    }
    // 支持 COMMA混用的 INNER JOIN 语法  
    | SELECT expression_list FROM relation INNER JOIN join_clauses where group_by
    {
      $$ = new ParsedSqlNode(SCF_SELECT);
      if ($2 != nullptr) {
        $$->selection.expressions.swap(*$2);
        delete $2;
      }

      if ($4 != nullptr) {
        $$->selection.relations.emplace_back($4);
        free($4);
      }

      if ($7 != nullptr) {
        for (auto it = $7->relations.rbegin(); it != $7->relations.rend(); ++it) {
          $$->selection.relations.emplace_back(std::move(*it));
        }
        $$->selection.conditions = std::move($7->conditions);
      }

      if ($8 != nullptr) {
        auto ptr = $$->selection.conditions.release();
        $$->selection.conditions = std::make_unique<ConjunctionExpr>(ConjunctionExpr::Type::AND, ptr, $8);
      }

      if ($9 != nullptr) {
        $$->selection.group_by.swap(*$9);
        delete $9;
      }
    }
    ;

join_clauses:
    relation ON condition
    {
      $$ = new JoinSqlNode;
      $$->relations.emplace_back($1);
      $$->conditions = std::unique_ptr<Expression>($3);
      free($1);
    }
    | relation ON condition INNER JOIN join_clauses
    {
      $$ = $6;
      $$->relations.emplace_back($1);
      auto ptr = $$->conditions.release();
      $$->conditions = std::make_unique<ConjunctionExpr>(ConjunctionExpr::Type::AND, ptr, $3);
      free($1);
    }
    | relation ON condition COMMA relation
    {
      $$ = new JoinSqlNode;
      $$->relations.emplace_back($1);
      $$->relations.emplace_back($5);
      $$->conditions = std::unique_ptr<Expression>($3);
      free($1);
      free($5);
    }
    ;
calc_stmt:
    CALC expression_list
    {
      $$ = new ParsedSqlNode(SCF_CALC);
      $$->calc.expressions.swap(*$2);
      delete $2;
    }
    | SELECT expression_list
    {
      $$ = new ParsedSqlNode(SCF_CALC);
      $$->calc.expressions.swap(*$2);
      delete $2;
    }
    ;

alias:
    /* empty */ {
      $$ = nullptr;
    }
    | AS ID {
      $$ = $2;
    }
    | ID {
      $$ = $1;
    }
    ;

expression_list:
    /* empty */ {
      $$ = new vector<unique_ptr<Expression>>;
    }
    | expression alias
    {
      $$ = new vector<unique_ptr<Expression>>;
      if (nullptr != $2) {
        $1->set_alias($2);
      }
      $$->emplace_back($1);
    }
    | expression alias COMMA expression_list
    {
      if ($4 != nullptr) {
        $$ = $4;
      } else {
        $$ = new vector<unique_ptr<Expression>>;
      }
      if (nullptr != $2) {
        $1->set_alias($2);
      }
      $$->emplace($$->begin(), $1);
    }
    ;
expression:
    expression '+' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::ADD, $1, $3, sql_string, &@$);
    }
    | expression '-' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::SUB, $1, $3, sql_string, &@$);
    }
    | expression '*' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::MUL, $1, $3, sql_string, &@$);
    }
    | expression '/' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::DIV, $1, $3, sql_string, &@$);
    }
    | '-' expression %prec UMINUS {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::NEGATIVE, $2, nullptr, sql_string, &@$); //  官方故意写的BUG?? 表达式应该放在右边 符合逻辑
    }
    | value {
      $$ = new ValueExpr(*$1);
      $$->set_name(token_name(sql_string, &@$));
      delete $1;
    }
    | '*' {
      $$ = new StarExpr();
    }
    | ID DOT '*' {
      $$ = new StarExpr($1);
    }
    | LBRACE expression_list RBRACE  {
      if ($2->size() == 1) {
        $$ = $2->front().release();
      } else {
        $$ = new ListExpr(std::move(*$2));
      }
      delete $2;
      $$->set_name(token_name(sql_string, &@$));
    }
    | rel_attr {
      RelAttrSqlNode *node = $1;
      $$ = new UnboundFieldExpr(node->relation_name, node->attribute_name);
      $$->set_name(token_name(sql_string, &@$));
      delete $1;
    }
    | function_expression {
      $$ = $1;
    }
    | LBRACE select_stmt RBRACE
    {
      $$ = new SubQueryExpr($2->selection);
      $$->set_name(token_name(sql_string, &@$));
      delete $2;
    }
    ;

function_expression:
    // to be added later
    ID LBRACE expression_list RBRACE
    {
        $$ = new UnboundFunctionExpr($1, std::move(*$3));
        $$->set_name(token_name(sql_string, &@$));
        delete $3;
    }
    | ID LBRACE expression_list RBRACE AGAINST LBRACE SSS RBRACE {  
        $$ = new UnboundFunctionExpr($1, std::move(*$3));
        $$->set_name(token_name(sql_string, &@$));
        delete $3;
    }
    ;

rel_attr:
    ID {
      $$ = new RelAttrSqlNode;
      $$->attribute_name = $1;
    }
    | ID DOT ID {
      $$ = new RelAttrSqlNode;
      $$->relation_name  = $1;
      $$->attribute_name = $3;
    } 
    ;

relation:
    ID {
      $$ = $1;
    }
    ;

rel_list:
    relation alias {
      $$ = new std::vector<RelationNode>();
      if(nullptr!=$2){
        $$->emplace_back($1,$2);
      }else{
        $$->emplace_back($1);
      }
    }
    | relation alias COMMA rel_list {
      if ($4 != nullptr) {
        $$ = $4;
      } else {
        $$ = new std::vector<RelationNode>;
      }
      if(nullptr!=$2){
        $$->insert($$->begin(), RelationNode($1,$2));
      }else{
        $$->insert($$->begin(), RelationNode($1));
      }
    }
    ;

where:
    /* empty */
    {
      $$ = nullptr;
    }
    | WHERE condition {
      $$ = $2;  
    }
    ;

condition:
    expression comp_op expression {
      $$ = new ComparisonExpr($2, $1, $3);
    }
    | comp_op expression
    {
      Value val;
      val.set_null(true);
      ValueExpr *temp_expr = new ValueExpr(val);
      $$ = new ComparisonExpr($1, temp_expr, $2);
    }
    | condition AND condition
    {
      $$ = new ConjunctionExpr(ConjunctionExpr::Type::AND, $1, $3);
    }
    | condition OR condition
    {
      $$ = new ConjunctionExpr(ConjunctionExpr::Type::OR, $1, $3);
    }
    ;

having_condition:
    /* empty */
    {
      $$ = nullptr;
    }
    | HAVING condition {
      $$ = $2;  
    }
    ;

comp_op:
      EQ { $$ = EQUAL_TO; }
    | LT { $$ = LESS_THAN; }
    | GT { $$ = GREAT_THAN; }
    | LE { $$ = LESS_EQUAL; }
    | GE { $$ = GREAT_EQUAL; }
    | NE { $$ = NOT_EQUAL; }
    | LIKE { $$ = LIKE_OP; }
    | NOT LIKE { $$ = NOT_LIKE_OP; }
    | IS { $$ = IS_OP; }
    | IS NOT { $$ = IS_NOT_OP; }
    | IN { $$ = IN_OP; }
    | NOT IN { $$ = NOT_IN_OP; }
    | EXISTS { $$ = EXISTS_OP; }
    | NOT EXISTS { $$ = NOT_EXISTS_OP; }
    ;

// your code here
opt_order_by:
	/* empty */
    {
      $$ = nullptr;
    }
    | ORDER BY sort_list
    {
      $$ = $3;
      std::reverse($$->begin(),$$->end());
    }
    ;

opt_limit:
    /* empty */ {
      $$ = nullptr;
    }
    | LIMIT NUMBER
    {
      $$ = new LimitSqlNode();
      $$->limit = $2;
    }
    ;

sort_list:
	  sort_unit
	{
      $$ = new std::vector<OrderBySqlNode>;
      $$->emplace_back(std::move(*$1));
      delete $1;
	}
    | sort_unit COMMA sort_list
	{
      $3->emplace_back(std::move(*$1));
      $$ = $3;
      delete $1;
	}
	;

sort_unit:
	  expression
	{
      $$ = new OrderBySqlNode();
      $$->expr = std::unique_ptr<Expression>($1);
      $$->is_asc = true;
	}
	| expression DESC
	{
      $$ = new OrderBySqlNode();
      $$->expr = std::unique_ptr<Expression>($1);
      $$->is_asc = false;
	}
	| expression ASC
	{
      $$ = new OrderBySqlNode(); // 默认升序
      $$->expr = std::unique_ptr<Expression>($1);
      $$->is_asc = true;
	}
	;


group_by:
    /* empty */
    {
      $$ = nullptr;
    }
    | GROUP BY expression_list
    {
      // group by 的表达式范围与select查询值的表达式范围是不同的，比如group by不支持 *
      // 但是这里没有处理。
      $$ = $3;
    }
    ;
load_data_stmt:
    LOAD DATA INFILE SSS INTO TABLE ID fields_terminated_by enclosed_by
    {
      char *tmp_file_name = unescape_sql_string($4);
      
      $$ = new ParsedSqlNode(SCF_LOAD_DATA);
      $$->load_data.relation_name = $7;
      $$->load_data.file_name = tmp_file_name;
      if ($8 != nullptr) {
        char *tmp = unescape_sql_string($8);
        $$->load_data.terminated = tmp;
        free(tmp);
      }
      if ($9 != nullptr) {
        char *tmp = unescape_sql_string($9);
        $$->load_data.enclosed = tmp;
        free(tmp);
      }
      free(tmp_file_name);
    }
    ;

fields_terminated_by:
    /* empty */
    {
      $$ = nullptr;
    }
    | FIELDS TERMINATED BY SSS
    {
      $$ = $4;
    };

enclosed_by:
    /* empty */
    {
      $$ = nullptr;
    }
    | ENCLOSED BY SSS
    {
      $$ = $3;
    };

explain_stmt:
    EXPLAIN command_wrapper
    {
      $$ = new ParsedSqlNode(SCF_EXPLAIN);
      $$->explain.sql_node = unique_ptr<ParsedSqlNode>($2);
    }
    ;

set_variable_stmt:
    SET ID EQ value
    {
      $$ = new ParsedSqlNode(SCF_SET_VARIABLE);
      $$->set_variable.name  = $2;
      $$->set_variable.value = *$4;
      delete $4;
    }
    ;

opt_semicolon: /*empty*/
    | SEMICOLON
    ;
%%
//_____________________________________________________________________
extern void scan_string(const char *str, yyscan_t scanner);

int sql_parse(const char *s, ParsedSqlResult *sql_result) {
  yyscan_t scanner;
  std::vector<char *> allocated_strings;
  yylex_init_extra(static_cast<void*>(&allocated_strings),&scanner);
  scan_string(s, scanner);
  int result = yyparse(s, sql_result, scanner);

  for (char *ptr : allocated_strings) {
    free(ptr);
  }
  allocated_strings.clear();

  yylex_destroy(scanner);
  return result;
}

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

using namespace std;

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

VecDistanceExpr *create_distance_expression(const char *distance_type,
                                             Expression *left,
                                             Expression *right,
                                             const char *sql_string,
                                             YYLTYPE *llocp)
{
  std::string type_str(distance_type);
  std::cout<<"Distance type: " << type_str << std::endl;
  VecDistanceExpr::Type type;
  if(type_str == "EUCLIDEAN") {
    // L2
    type = VecDistanceExpr::Type::L2;
  } else if(type_str == "COSINE") {
    // COSINE
    type = VecDistanceExpr::Type::COSINE;
  } else if(type_str == "DOT") {
    // INNER
    type = VecDistanceExpr::Type::INNER;
  } else {
    LOG_ERROR("Unsupported distance type: %s", distance_type);
    return nullptr;
  }
  VecDistanceExpr *expr = new VecDistanceExpr(type, left, right);
  expr->set_name(token_name(sql_string, llocp));
  return expr;
}

FunctionExpr *create_function_expression(const char *function_type,
                                             Expression *child,
                                             const char *sql_string,
                                             int round,
                                             YYLTYPE *llocp)
{
  std::string type_str(function_type);
    std::cout<<"Function type: " << type_str << std::endl;
  FunctionExpr::Type type;
  if(type_str == "LENGTH") {
    // L2
    type = FunctionExpr::Type::LENGTH;
  } else if(type_str == "ROUND") {
    // COSINE
    type = FunctionExpr::Type::ROUND;
  } else if(type_str == "DATE_FORMAT") {
    // INNER
    type = FunctionExpr::Type::DATE_FORMAT;
  } else {
    LOG_ERROR("Unsupported function type: %s", function_type);
    return nullptr;
  }
  FunctionExpr *expr = new FunctionExpr(type, child, round);
  expr->set_name(token_name(sql_string, llocp));
  return expr;
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

UnboundAggregateExpr *create_aggregate_expression(const char *aggregate_name,
                                           Expression *child,
                                           const char *sql_string,
                                           YYLTYPE *llocp)
{
  UnboundAggregateExpr *expr = new UnboundAggregateExpr(aggregate_name, child);
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
        HELP
        EXIT
        DOT //QUOTE
        INTO
        VALUES
        FROM
        WHERE
        AND
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
        DISTANCE
        L2_DISTANCE
        COSINE_DISTANCE
        INNER_PRODUCT_DISTANCE
        VECTOR_TO_STRING
        STRING_TO_VECTOR
        NULL_T
        NULLABLE
        IS
        IN
        AS
        HAVING
        TEXT_T
        ROUND
        LENGTH
        DATE_FORMAT
        LIMIT

/** union 中定义各种数据类型，真实生成的代码也是union类型，所以不能有非POD类型的数据 **/
%union {
  ParsedSqlNode *                            sql_node;
  ConditionSqlNode *                         condition;
  Value *                                    value;
  enum CompOp                                comp;
  RelAttrSqlNode *                           rel_attr;
  vector<AttrInfoSqlNode> *                  attr_infos;
  AttrInfoSqlNode *                          attr_info;
  Expression *                               expression;
  vector<unique_ptr<Expression>> *           expression_list;
  vector<Value> *                            value_list;
  vector<ConditionSqlNode> *                 condition_list;
  vector<RelAttrSqlNode> *                   rel_attr_list;
  vector<RelationNode> *                     relation_list;
  vector<string> *                           key_list;
  OrderBySqlNode *                           orderby_unit;
  std::vector<OrderBySqlNode> *              orderby_list;
  LimitSqlNode *                             limit_node;
  char *                                     cstring;
  int                                        number;
  float                                      floats;
  bool                                       nullable_info;
  bool                                       unique;
  vector<UpdateField> *                      update_list;
}

%destructor { delete $$; } <condition>
%destructor { delete $$; } <value>
%destructor { delete $$; } <rel_attr>
%destructor { delete $$; } <attr_infos>
%destructor { delete $$; } <expression>
%destructor { delete $$; } <expression_list>
%destructor { delete $$; } <value_list>
%destructor { delete $$; } <condition_list>
// %destructor { delete $$; } <rel_attr_list>
%destructor { delete $$; } <relation_list>
%destructor { delete $$; } <key_list>

%token <number> NUMBER
%token <floats> FLOAT
%token <cstring> ID
%token <cstring> VECTOR
%token <cstring> SSS
%token <cstring> DISTANCE_TYPE
%token <cstring> DATE
//非终结符

/** type 定义了各种解析后的结果输出的是什么类型。类型对应了 union 中的定义的成员变量名称 **/
%type <number>              type
%type <condition>           condition
%type <value>               value
%type <number>              number
%type <cstring>             relation
%type <comp>                comp_op
%type <rel_attr>            rel_attr
%type <nullable_info>       nullable_constraint
%type <attr_infos>          attr_def_list
%type <attr_info>           attr_def
%type <value_list>          value_list
%type <condition_list>      where
%type <condition_list>      condition_list
%type <condition_list>      having_condition
%type <cstring>             storage_format
%type <key_list>            primary_key
%type <key_list>            attr_list
%type <relation_list>       rel_list
%type <expression>          expression
%type <expression>          aggregate_expression
%type <expression>          vector_expression
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
%type <sql_node>            calc_stmt
%type <sql_node>            select_stmt
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
%type <update_list>       update_list

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
        $$->length = 65535;
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

insert_stmt:        /*insert   语句的语法解析树*/
    INSERT INTO ID VALUES LBRACE value_list RBRACE 
    {
      $$ = new ParsedSqlNode(SCF_INSERT);
      $$->insertion.relation_name = $3;
      $$->insertion.values.swap(*$6);
      delete $6;
    }
    ;

value_list:
    value
    {
      $$ = new vector<Value>;
      $$->emplace_back(*$1);
      delete $1;
    }
    | value_list COMMA value { 
      $$ = $1;
      $$->emplace_back(*$3);
      delete $3;
    }
    ;
value:
    NUMBER {
      $$ = new Value((int)$1);
      @$ = @1;
    }
    | '-' NUMBER {
      $$ = new Value(-(int)$2);
      @$ = @1;
    }
    |FLOAT {
      $$ = new Value((float)$1);
      @$ = @1;
    }
    | '-' FLOAT {
      $$ = new Value(-(float)$2);
      @$ = @1;
    }

    |SSS {
      char *tmp = common::substr($1,1,strlen($1)-2);
      $$ = new Value(tmp);
      free(tmp);
    }
    |DATE {
      char *tmp = common::substr($1,1,strlen($1)-2);
      $$ = Value::from_date(tmp);
      // 在语法解析时检查，强制清空以触发FAILURE
      if (!$$->is_valid_date()) {
        $$->reset();
      }
      free(tmp);
      free($1);
    }
    |NULL_T {
      $$ = new Value();
      $$->set_null();
    }
    |STRING_TO_VECTOR LBRACE VECTOR RBRACE {
      if ($3[0] =='\'' || $3[0] == '\"') {
        // 去掉引号
        char *tmp = common::substr($3,1,strlen($3)-2);
        
        $$ = Value::string_to_vector(tmp);
        free(tmp);
      } else {
        $$ = Value::string_to_vector($3);
      }
      free($3);
    }
    |VECTOR{
      if ($1[0] =='\'' || $1[0] == '\"') {
        // 去掉引号
        char *tmp = common::substr($1,1,strlen($1)-2);
        
        $$ = Value::string_to_vector(tmp);
        free(tmp);
      } else {
        $$ = Value::string_to_vector($1);
      }
      free($1);
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
        $$->deletion.conditions.swap(*$4);
        delete $4;
      }
    }
    ;
update_stmt:      /*  update 语句的语法解析树*/
    // UPDATE ID SET ID EQ value where 
    // {
    //   $$ = new ParsedSqlNode(SCF_UPDATE);
    //   $$->update.relation_name = $2;
    //   $$->update.attribute_name = $4;
    //   $$->update.value = *$6;
    //   if ($7 != nullptr) {
    //     $$->update.conditions.swap(*$7);
    //     delete $7;
    //   }
    // }
    UPDATE ID SET update_list where 
    {
      $$ = new ParsedSqlNode(SCF_UPDATE);
      $$->update.relation_name = $2;
      if ($4 != nullptr) {
        $$->update.update_list.swap(*$4);
        delete $4;
      }
      if ($5 != nullptr) {
        $$->update.conditions.swap(*$5);
        delete $5;
      }
    }
    ;
update_list:
    ID EQ value{
      $$ = new vector<UpdateField>;
      UpdateField update;
      update.attribute_name = $1;
      update.value = *$3;
      $$->push_back(update);
      delete $3;
    }
    | update_list COMMA ID EQ value{
      $$ = $1;
      UpdateField update;
      update.attribute_name = $3;
      update.value = *$5;
      $$->push_back(update);
      delete $5;
    }
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
        $$->selection.conditions.swap(*$5);
        delete $5;
      }

      if ($6 != nullptr) {
        $$->selection.group_by.swap(*$6);
        delete $6;
      }

      if( $7 != nullptr) {
        $$->selection.having_conditions.swap(*$7);
        delete $7;
      }

      if ($8 != nullptr) {
        $$->selection.order_by.swap(*$8);
        delete $8;
      }

      if ($9 != nullptr) {
        $$->selection.limit = $9->limit_count;
        delete $9;
      }
    }
    | SELECT expression_list
    {
      $$ = new ParsedSqlNode(SCF_SELECT);
      if ($2 != nullptr) {
        $$->selection.expressions.swap(*$2);
        delete $2;
      }
    }
    ;
calc_stmt:
    CALC expression_list
    {
      $$ = new ParsedSqlNode(SCF_CALC);
      $$->calc.expressions.swap(*$2);
      delete $2;
    }
    ;

alias:
    AS ID {
      $$ = $2;
    }
    | ID {
      $$ = $1;
    }
    ;

expression_list:
    expression
    {
      $$ = new vector<unique_ptr<Expression>>;
      $$->emplace_back($1);
    }
    | expression COMMA expression_list
    {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new vector<unique_ptr<Expression>>;
      }
      $$->emplace($$->begin(), $1);
    }
    |expression alias
    {
      $$ = new vector<unique_ptr<Expression>>;
      $1->set_field_alias($2);
      $$->emplace_back($1);
    }
    | expression alias COMMA expression_list
    {
      if ($4 != nullptr) {
        $$ = $4;
      } else {
        $$ = new vector<unique_ptr<Expression>>;
      }
      $1->set_field_alias($2);
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
    | LBRACE select_stmt RBRACE
    {
      $$ = new SubqueryExpr($2);
      $$->set_name(token_name(sql_string, &@$));
    }
    | LBRACE expression RBRACE {
      $$ = $2;
      $$->set_name(token_name(sql_string, &@$));
    }
    | '-' expression %prec UMINUS {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::NEGATIVE, nullptr, $2, sql_string, &@$); //  官方故意写的BUG?? 表达式应该放在右边 符合逻辑
    }
    | '*' {
      $$ = new StarExpr();
    }
    | ID DOT '*' {
      $$ = new StarExpr($1);
    }
    | value {
      $$ = new ValueExpr(*$1);
      $$->set_name(token_name(sql_string, &@$));
      delete $1;
    }
    | LBRACE value_list RBRACE  {
      std::vector<Value> *values = $2;
      $$ = new ValueListExpr(*values);
      $$->set_name(token_name(sql_string, &@$));
    }
    | rel_attr {
      RelAttrSqlNode *node = $1;
      $$ = new UnboundFieldExpr(node->relation_name, node->attribute_name);
      $$->set_name(token_name(sql_string, &@$));
      delete $1;
    }
    | aggregate_expression {
      $$ = $1;
    }
    | vector_expression {
      $$ = $1;
    }
    | function_expression {
      $$ = $1;
    }
    ;

aggregate_expression:
    ID LBRACE expression RBRACE {
      $$ = create_aggregate_expression($1, $3, sql_string, &@$);
    }
    // your code here
    | ID LBRACE expression_list RBRACE{
      $$ = new UnboundAggregateExpr("max", new StarExpr());
    }
    | ID LBRACE RBRACE{
      $$ = new UnboundAggregateExpr("max", new StarExpr());
    }
    ;

vector_expression:
    DISTANCE LBRACE expression COMMA expression COMMA DISTANCE_TYPE RBRACE
    {
      char * tmp = common::substr($7,1,strlen($7)-2);
      $$ = create_distance_expression(tmp, $3, $5, sql_string, &@$);
      free(tmp);
    }
    | L2_DISTANCE LBRACE expression COMMA expression RBRACE
    {
      $$ = create_distance_expression("EUCLIDEAN", $3, $5, sql_string, &@$);
    }
    | COSINE_DISTANCE LBRACE expression COMMA expression RBRACE
    {
      $$ = create_distance_expression("COSINE", $3, $5, sql_string, &@$);
    }
    | INNER_PRODUCT_DISTANCE LBRACE expression COMMA expression RBRACE
    {
      $$ = create_distance_expression("DOT", $3, $5, sql_string, &@$);
    }
    | VECTOR_TO_STRING LBRACE expression RBRACE
    {
      $$ = new VectorToStringExpr($3);
      $$->set_name(token_name(sql_string, &@$));
    }
    ;

function_expression:
    // to be added later
    | LENGTH LBRACE expression RBRACE {
      $$ = create_function_expression("LENGTH", $3, sql_string, 0, &@$);
    }
    | ROUND LBRACE expression RBRACE {
      $$ = create_function_expression("ROUND", $3, sql_string, 0, &@$);
    }
    | ROUND LBRACE expression COMMA NUMBER RBRACE {
      int round = $5;
      $$ = create_function_expression("ROUND", $3, sql_string, round, &@$);
    }
    | DATE_FORMAT LBRACE expression COMMA SSS RBRACE {

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
    relation {
      $$ = new vector<RelationNode>();
      $$->emplace_back($1);
    }
    | relation COMMA rel_list {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new vector<RelationNode>();
      }

      $$->insert($$->begin(), RelationNode($1) );
    }
    | relation alias{
      $$ = new vector<RelationNode>();
      $$->emplace_back($1,$2);
    }
    | relation alias COMMA rel_list {
      if ($4 != nullptr) {
        $$ = $4;
      } else {
        $$ = new vector<RelationNode>();
      }

      $$->insert($$->begin(), RelationNode($1,$2) );
    }
    ;

where:
    /* empty */
    {
      $$ = nullptr;
    }
    | WHERE condition_list {
      $$ = $2;  
    }
    ;
condition_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | condition {
      $$ = new vector<ConditionSqlNode>;
      $$->emplace_back(std::move(*$1)); // 由于Condition中有不可Copy的unique_ptr成员，所以这里必须用move语义
      delete $1;
    }
    | condition AND condition_list {
      $$ = $3;
      $$->emplace_back(std::move(*$1));
      delete $1;
    }
    ;
condition:
    expression comp_op expression {
      $$ = new ConditionSqlNode;
      $$->left = std::unique_ptr<Expression>($1);
      $$->right = std::unique_ptr<Expression>($3);
      $$->comp = $2;
    }
    ;

having_condition:
    /* empty */
    {
      $$ = nullptr;
    }
    | HAVING condition_list {
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
    {
      $$ = nullptr;
    }
    | LIMIT NUMBER
    {
      $$ = new LimitSqlNode($2);
    }
    ;

sort_list:
	  sort_unit
	{
      $$ = new std::vector<OrderBySqlNode>;
      $$->emplace_back(std::move(*$1));
	}
    | sort_unit COMMA sort_list
	{
      $3->emplace_back(std::move(*$1));
      $$ = $3;
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
      char *tmp_file_name = common::substr($4, 1, strlen($4) - 2);
      
      $$ = new ParsedSqlNode(SCF_LOAD_DATA);
      $$->load_data.relation_name = $7;
      $$->load_data.file_name = tmp_file_name;
      if ($8 != nullptr) {
        char *tmp = common::substr($8,1,strlen($8)-2);
        $$->load_data.terminated = $8;
        free(tmp);
      }
      if ($9 != nullptr) {
        char *tmp = common::substr($9,1,strlen($9)-2);
        $$->load_data.enclosed = $9;
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
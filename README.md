# sldb

本项目是一个以学习为目的的关系型数据库，目标是实现一个简易的关系数据库，包括数据库的基本功能，如SQL、事务、并发控制、存储引擎等。


## 需求

初期需求：
- 支持基本的SELECT/INSERT/UPDATE/DELETE语句
- 支持CTEATE/DROP TABLE语句
- 支持基本的数据类型（整数，字符串）


## 开发计划

1. 首先开发解析器，实现解析基本的SELECT/INSERT/UPDATE/DELETE语句,CTEATE/DROP TABLE语句


## 开发环境
```sh
make              # 构建
make test         # 跑全部测试用例
echo "SELECT a from t1;" | ./sldb
```
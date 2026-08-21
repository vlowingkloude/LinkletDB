# LinkletDB

```bash
mkdir build

cd build

cmake -DCMAKE_BUILD_TYPE=Release -S .. -B .

cmake --build . --target linklet_shell
```

### Structure

Query -parser-> AST -planner/binder-> Logical Plan -executor and advisor-> Result


### Example

```bash
./linklet_shell myg

# or

./linklet_shell myg myg
```

```gql
CREATE GRAPH TYPE social { (Person {name STRING, age INT}), EDGE KNOWS }

INSERT (alice:Person {name: 'Alice', age: 30})
INSERT (bob:Person {name: 'Bob', age: 25, active: TRUE})
INSERT (carol:Person {name: 'Carol', age: 28})
INSERT (dora:Person {name: 'Dora'})
INSERT (eve:Person {name: 'Eve', age: 31})

MATCH (a {id: 0}), (b {id: 1}) INSERT (a)-[e:KNOWS {since: 2020}]->(b)
MATCH (a {id: 1}), (b {id: 2}) INSERT (a)-[e:KNOWS {since: 2021}]->(b)
MATCH (a {id: 2}), (b {id: 3}) INSERT (a)-[e:KNOWS {since: 2022}]->(b)
MATCH (a {id: 3}), (b {id: 4}) INSERT (a)-[e:KNOWS {since: 2023}]->(b)
MATCH (a {id: 0}), (b {id: 4}) INSERT (a)-[e:KNOWS {since: 2024, strength: 0.8}]->(b)

MATCH (n:Person) RETURN n
MATCH (n:Person {name: 'Alice'}) RETURN n
MATCH (a:Person {name: 'Alice'})-[e:KNOWS {since: 2020}]->(b:Person {name: 'Bob'}) RETURN e

MATCH (a {id: 0})-[e]->{1,3}(b {id: 3}) RETURN b
MATCH (a {id: 3})<-[e]-{1,3}(b {id: 0}) RETURN b

MATCH (n:Person {name: 'Alice'}) SET n.name = 'Alicia', n.bio = 'graph researcher' RETURN n
MATCH (n:Person {name: 'Alicia', age: 30}) RETURN n

BEGIN
INSERT (frank:Person {name: 'Frank', age: 40})
INSERT (grace:Person {name: 'Grace', age: 35})
COMMIT
BEGIN
INSERT (hank:Person {name: 'Hank'})
ROLLBACK
MATCH (n:Person {name: 'Hank'}) RETURN n

?MATCH (n:Person {name: 'Frank'}) RETURN n

MATCH (a {id: 0})-[e:KNOWS {since: 2020}]->(b {id: 1}) DELETE e
MATCH (n {id: 1}) DETACH DELETE n
MATCH (n:Person) RETURN n

```
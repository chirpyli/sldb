SELECT id, name FROM users WHERE age > 18 ORDER BY id DESC LIMIT 10;

SELECT * FROM orders WHERE total >= 100 AND status = 'paid';

SELECT a + b AS sum, c FROM t WHERE NOT (x = 1 OR y = 2);

DELIMITER //

CREATE PROCEDURE CopyRecordsWithTenantId(specific_tenant_id INT)
BEGIN
    DECLARE done INT DEFAULT 0;
    DECLARE tableName VARCHAR(255);
    DECLARE cursor CURSOR FOR 
        SELECT TABLE_NAME 
        FROM INFORMATION_SCHEMA.TABLES 
        WHERE TABLE_SCHEMA = DATABASE();
    DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = 1;

    OPEN cursor;

    read_loop: LOOP
        FETCH cursor INTO tableName;
        IF done THEN
            LEAVE read_loop;
        END IF;

        SET @query = CONCAT(
            'INSERT INTO ', tableName, ' (',
            'SELECT * FROM ', tableName,
            ' WHERE tenant_id = ?) AS source'
        );

        SET @modify_query = CONCAT(
            'UPDATE ', tableName, ' SET tenant_id = 0 WHERE tenant_id = ?'
        );

        SET @tenant_id = specific_tenant_id;
        PREPARE stmt FROM @query;
        EXECUTE stmt USING @tenant_id;
        DEALLOCATE PREPARE stmt;

        PREPARE stmt2 FROM @modify_query;
        EXECUTE stmt2 USING @tenant_id;
        DEALLOCATE PREPARE stmt2;

    END LOOP;

    CLOSE cursor;
END;
 //

DELIMITER ;


DELIMITER //

CREATE PROCEDURE CopyRecordsWithTenantId(IN specific_tenant_id INT)
BEGIN
    DECLARE done INT DEFAULT 0;
    DECLARE tableCount INT DEFAULT 0;
    DECLARE i INT DEFAULT 0;

    -- Create a temporary table to store the table names
    CREATE TEMPORARY TABLE TempTables (tableName VARCHAR(255));

    -- Populate the temporary table with the relevant table names
    INSERT INTO TempTables (tableName)
    SELECT TABLE_NAME 
    FROM INFORMATION_SCHEMA.TABLES 
    WHERE TABLE_SCHEMA = DATABASE();

    -- Get the count of tables
    SELECT COUNT(*) INTO tableCount FROM TempTables;

    -- Loop through each table in the temporary table
    WHILE i < tableCount DO
        SET i = i + 1;

        -- Construct the SQL query to fetch the table name with the dynamic offset
        SET @sql = CONCAT('SELECT tableName INTO @currentTableName FROM TempTables LIMIT 1 OFFSET ', i - 1);
        PREPARE stmt FROM @sql;
        EXECUTE stmt;
        DEALLOCATE PREPARE stmt;

        -- Now @currentTableName contains the table name for the current iteration
        -- Construct the insert and update queries using @currentTableName
        SET @main_query = CONCAT(
            'INSERT INTO ', @currentTableName, 
            ' (SELECT * FROM ', @currentTableName,
            ' WHERE tenant_id = ', specific_tenant_id, ')'
        );

        SET @modify_query = CONCAT(
            'UPDATE ', @currentTableName, 
            ' SET tenant_id = 0 WHERE tenant_id = ', specific_tenant_id
        );

        -- Execute the insert and update statements
        PREPARE stmt FROM @main_query;
        EXECUTE stmt;
        DEALLOCATE PREPARE stmt;

        PREPARE stmt2 FROM @modify_query;
        EXECUTE stmt2;
        DEALLOCATE PREPARE stmt2;

    END WHILE;

    -- Drop the temporary table
    DROP TEMPORARY TABLE IF EXISTS TempTables;

END;
//

DELIMITER ;


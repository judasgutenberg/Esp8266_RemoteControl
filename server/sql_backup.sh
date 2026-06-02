#!/bin/bash

# Usage: ./backup_script.sh username password db_name path "table1 table2 table3" "table4 table5"

# Database credentials
DB_USER=($1)
DB_PASSWORD=($2)
DB_NAME=($3)
BACKUP_DIR=($4)

# Tables for schema only (passed as first argument)
SCHEMA_ONLY_TABLES=($5)

# Tables for data with WHERE clause (passed as second argument)
DATA_TABLES=($6)
WHERE_CLAUSE="tenant_id=0"

# File paths
SCHEMA_ONLY_FILE="${BACKUP_DIR}/schema_only.sql"
DATA_FILE="${BACKUP_DIR}/data_with_where.sql"
FINAL_BACKUP_FILE="./remote_control.sql"

# Create backup directory if it doesn't exist
mkdir -p ${BACKUP_DIR}

# Clear old backup files if they exist
> ${SCHEMA_ONLY_FILE}
> ${DATA_FILE}

# Backup schema only tables
for TABLE in "${SCHEMA_ONLY_TABLES[@]}"; do
    echo "Backing up schema for table: ${TABLE}"
    mysqldump --user=${DB_USER} --password=${DB_PASSWORD} --no-data ${DB_NAME} ${TABLE} >> ${SCHEMA_ONLY_FILE}
done

# Backup data tables with WHERE clause
for TABLE in "${DATA_TABLES[@]}"; do
    echo "Backing up data for table: ${TABLE} with condition ${WHERE_CLAUSE}"
    mysqldump --user=${DB_USER} --password=${DB_PASSWORD} --no-create-info --where="${WHERE_CLAUSE}" ${DB_NAME} ${TABLE} >> ${DATA_FILE}
done

# Combine schema and data dumps
cat ${SCHEMA_ONLY_FILE} ${DATA_FILE} > ${FINAL_BACKUP_FILE}

# Optional: Clean up intermediate files
rm ${SCHEMA_ONLY_FILE} ${DATA_FILE}

echo "Backup completed. Final backup file is ${FINAL_BACKUP_FILE}"

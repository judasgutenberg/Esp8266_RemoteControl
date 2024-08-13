#!/bin/bash

# Usage: ./full_sql_backup.sh username password db_name path  

# Database credentials
DB_USER=($1)
DB_PASSWORD=($2)
DB_NAME=($3)
BACKUP_LOC=($4)

mysqldump --user=${DB_USER} --password=${DB_PASSWORD} ${DB_NAME} >> ${BACKUP_LOC}

#!/bin/sh

IRODS_ADMIN_USER="irodsmaster";
LOG_FILE="/tmp/accountCreation.log";
HOME="/home/irodsmaster";
export HOME;
IRODS_ENVIRONMENT_FILE="/home/irodsmaster/.irods/irods_environment.json";
export IRODS_ENVIRONMENT_FILE;
#PAM_USER="Guybrush";

date >> ${LOG_FILE};
echo "PAM user: " $PAM_USER >> ${LOG_FILE};
echo "user:" $(whoami) >> ${LOG_FILE};
if [ $(whoami) != ${IRODS_ADMIN_USER} ]; then
    echo "The program is not executed by the iRODS admin user: ${IRODS_ADMIN_USER}" >> ${LOG_FILE};
    exit 1;
fi

if [ -z $PAM_USER ]; then
    echo "The parameter PAM_USER is unset or empty" >> ${LOG_FILE};
    exit 1;
fi

result=`/bin/iadmin lu "$PAM_USER"`;
if [ "$result" == "No rows found" ]; then
    iadmin mkuser "$PAM_USER" rodsuser;
    result=`/bin/iadmin lu "$PAM_USER"`;
fi

if [ "$result" == "No rows found" ]; then
    echo "Impossible to create an account for user $PAM_USER" >> ${LOG_FILE};
    exit 1;
fi

exit 0;

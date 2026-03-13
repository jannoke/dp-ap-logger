#ifndef DPAPLOGGER_H_
#define DPAPLOGGER_H_

#include <iostream>
#include <mysql++/mysql++.h>
#include <signal.h>
#include <map>
#include <time.h>
#include <sys/stat.h>

#include "includes/ConfigFile/ConfigFile.h"
#include "includes/pstreams/pstream.h"
#include "config.h"

using namespace std;

/* This is the MySQL database's vhost_bw table structure */
struct bandwidth_data {
	unsigned int 	id;
	unsigned int 	domain_id;
	unsigned int 	date;
	unsigned long 	rcvd;
	unsigned long 	sent;
	unsigned int    time;
};

/* Contains information about a domain (virtualhost) log files and
 * handles */
struct domain_loghandles {
	fstream *user_handle;
	fstream *handle;
	string 	*user_logfile;
	string 	*logfile;
};

struct domain_data {
	unsigned int 	domain_id;
	string 			domainname;
	string 			basepath;
	uid_t 			uid;
	gid_t 			gid;
	
	struct domain_loghandles handles;
};

struct line_data {
	unsigned long 	sent;
	unsigned long 	rcvd;
	
	time_t time;
	string domain;
	string line;
};

#endif /*DPAPLOGGER_H_*/

//============================================================================
// Name        : dp-ap-logger
// Author      : Taavi Sannik
// Version     : 1.1.2
// Copyright   : DataCode OU 2008
// Description : Qmail-inject from address authenticator & bandwidth counter
//============================================================================


#include "dp-ap-logger.h"

using namespace std;

static ConfigFile config;
static mysqlpp::Connection conn;
static string table_prefix, log_root, log_type, logrotate_cmd, default_logfile;
static __mode_t userlog_perm;
static int server_id;
static map<std::string, int> months;
static map<int, int> bw_cache;
static map<std::string, struct domain_data> avail_domains;
static redi::ipstream *logrotate_stream;
static bool logrotate_running; 

void wait_input() {
	while(cin.eof() && !cin.fail() && cin.good()) {
		usleep(500000);
	}
}

bool file_exists(string *filename) {
	struct stat file_stat;
	
	return stat(filename->c_str(), &file_stat) == 0;
}

void collect_domains() {
	mysqlpp::Query query = conn.query();
	struct domain_data d_data;
	
	avail_domains.clear();
	
	query << "SELECT d.domain_id, d.domainname, c.basepath, c.uid, c.gid FROM `" << table_prefix << "vhost` v" <<
			 "								JOIN `" << table_prefix << "domain` d USING (domain_id)" <<
			 "								JOIN `" << table_prefix << "client` c USING (webhost_client_id) WHERE " << 
			 "							v.server_id = " << server_id;
	
	mysqlpp::StoreQueryResult res = query.store();
	
	if (query.errnum() > 0) cerr << "dp-ap-logger: MySQL error: " << query.error() << endl;
	else if (res) {
		for(size_t i = 0; i < res.num_rows(); ++i) {
			
			d_data.domain_id 	= res[i]["domain_id"];
			d_data.domainname.assign(res[i]["domainname"]);
			d_data.basepath.assign(res[i]["basepath"]);
			d_data.uid 			= res[i]["uid"];
			d_data.gid 			= res[i]["gid"];
			
			memset(&d_data.handles, 0, sizeof(domain_loghandles));
			
			avail_domains[d_data.domainname] = d_data;
		}
	}
	
	d_data.domain_id 	= 0;
	d_data.domainname.assign("default");
	d_data.basepath.assign("");
	d_data.uid 			= 0;
	d_data.gid 			= 0;
	
	d_data.handles.handle 	 = new fstream();
	d_data.handles.handle->open(default_logfile.c_str(), fstream::out|fstream::app);
	
	d_data.handles.user_logfile = new string("");
	d_data.handles.logfile 		= new string(default_logfile);	
	
	avail_domains[d_data.domainname] = d_data;
	
	if (!d_data.handles.handle->is_open()) {
		cerr << "dp-ap-logger: failed to open default log file " << default_logfile << endl;
		return;
	}	

#ifdef DEBUG_ENABLED
	map<std::string, struct domain_data>::iterator iter;
	for(iter=avail_domains.begin(); iter!=avail_domains.end(); ++iter) {
		cout << iter->first << " : " << iter->second.domain_id << endl; 
	}
#endif
}

void update_domain_bandwidth(struct bandwidth_data *data) {
	mysqlpp::Query query = conn.query();
	
	if (bw_cache.count(data->domain_id) == 0) {
		query << "SELECT id FROM `" << table_prefix << "vhost_bw` WHERE " << 
				 "							domain_id = " << data->domain_id << " AND " <<
				 "							date = " << data->date << " LIMIT 1";
		
		mysqlpp::StoreQueryResult res = query.store();
		
		if (query.errnum() > 0) {
		    cerr << "dp-ap-logger: MySQL error: " << query.error() << endl;
		    return;
		}
		
		if (res && res.num_rows() == 1) {
			data->id = res[0]["id"];
			bw_cache[data->domain_id] = data->id;
#ifdef DEBUG_ENABLED
			cout << "found cache id: " << data->id << " domain_id: " << data->domain_id << endl;
#endif
		}
	}
	else {
		data->id = bw_cache[data->domain_id];
	}
	
	query = conn.query();
	
	if (data->id == 0) {
		query << "INSERT INTO `" << table_prefix << "vhost_bw` SET rcvd = " << data->rcvd << "," <<
				 "							sent = " << data->sent << "," <<
				 "							time = " << data->time << "," <<
				 "							domain_id = " << data->domain_id << "," <<
				 "							date = " << data->date << "," <<
				 "							count = 1";
	}
	else {
		query << "UPDATE `" << table_prefix << "vhost_bw` SET rcvd = rcvd + " << data->rcvd << "," <<
				 "							sent = sent + " << data->sent << "," <<
				 "							time = time + " << data->time << "," <<
				 "							count = count + 1" <<
				 "							WHERE id = " << data->id;
	}
	query.execute();
	
	if (query.errnum() > 0) {
	    cerr << "dp-ap-logger: MySQL error: " << query.error() << endl;
	    return;
	}
	
	if (data->id == 0) {
		data->id = query.insert_id();
		bw_cache[data->domain_id] = data->id;
#ifdef DEBUG_ENABLED
		cout << "created cache id: " << data->id << " domain_id: " << data->domain_id << endl;
#endif
	}
}

int get_date(time_t timestamp, bool short_date=false) {
	char date[8];
	struct tm* time_s;
	
	time_s = localtime(&timestamp);
	
	strftime(date, 10, short_date ? "%Y%m" : "%Y%m%d", time_s);	
	
	return atoi(date);
}

int get_current_date() {
	time_t timestamp;
	
	time(&timestamp);

	return get_date(timestamp);
}

void write_log(struct line_data *data, struct domain_data *d_data) {
	
	if (d_data->domainname != "default" && 
			(d_data->handles.handle == NULL || d_data->handles.user_handle == NULL)) {
		/*	Need to open log handles */
		stringstream *namestream;
		string logpath, user_logpath, cmd;
		
		/*	Build up logfile names */
		namestream = new stringstream();
		*namestream << log_root << "/" << d_data->gid << "/" << data->domain << "/" 
					<< data->domain << "_" << log_type << "_"
					<< get_date(data->time, (log_type == "error")) << ".log";
		*namestream >> logpath;
		delete namestream;
		
		namestream = new stringstream();
		*namestream << d_data->basepath << "/" << d_data->gid << "/logs/" << data->domain << "_" << log_type << ".log";
		*namestream >> user_logpath;
		delete namestream;
		
		/*	Open log handles */
		d_data->handles.handle 	 = new fstream();
		d_data->handles.user_handle = new fstream();
		
		d_data->handles.handle->open(logpath.c_str(), fstream::out|fstream::app);
		d_data->handles.user_handle->open(user_logpath.c_str(), fstream::out|fstream::app);		

		if (!d_data->handles.handle->is_open()) {
			cerr << "dp-ap-logger: failed to open log file " << logpath << endl;
			return;
		}
		
		if (!d_data->handles.user_handle->is_open()) {
			cerr << "dp-ap-logger: failed to open user log file " << user_logpath << endl;
			return;
		}
		else {
			chown(user_logpath.c_str(), d_data->uid, d_data->gid);
			chmod(user_logpath.c_str(), userlog_perm);
		}
		
		/* Store logfile names in case we ever need them */
		d_data->handles.user_logfile 	= new string(user_logpath);
		d_data->handles.logfile 		= new string(logpath);
	}
	
	/*	Write log lines */
	
	if (d_data->handles.handle->is_open() && d_data->handles.handle->good()) {
		*d_data->handles.handle << data->line << endl;
	}
	
	if (d_data->domainname != "default" && d_data->handles.user_handle->is_open() && d_data->handles.user_handle->good()) {
		*d_data->handles.user_handle << data->line << endl;
	}	
}

void init_months() {
	months["Jan"] = 1;
	months["Feb"] = 2;
	months["Mar"] = 3;
	months["Apr"] = 4;
	months["May"] = 5;
	months["Jun"] = 6;
	months["Jul"] = 7;
	months["Aug"] = 8;
	months["Sep"] = 9;
	months["Oct"] = 10;
	months["Nov"] = 11;
	months["Dec"] = 12;
}

int month_to_int(string month) {
	if (months.count(month) == 0) return 0;
	return months[month];
}

time_t parse_common_time(string *word) {
	struct tm tp;

	if (word->length() != 28) return -1;
	
	tp.tm_mday 	 = atoi(word->substr( 1,2).c_str());
	tp.tm_mon 	 = month_to_int(word->substr(4,3))-1;
	tp.tm_year 	 = atoi(word->substr( 8,4).c_str())-1900;
	tp.tm_hour 	 = atoi(word->substr(13,2).c_str());
	tp.tm_min 	 = atoi(word->substr(16,2).c_str());
	tp.tm_sec	 = atoi(word->substr(19,2).c_str());
	
	return mktime(&tp);
}

time_t parse_error_time(string *word) {
	struct tm tp;

	if (word->length() != 24) return -1;
	
	tp.tm_mon 	 = month_to_int(word->substr(4,3))-1;
	tp.tm_mday 	 = atoi(word->substr( 8,2).c_str());
	tp.tm_hour 	 = atoi(word->substr(11,2).c_str());
	tp.tm_min 	 = atoi(word->substr(14,2).c_str());
	tp.tm_sec	 = atoi(word->substr(17,2).c_str());
	tp.tm_year 	 = atoi(word->substr(20,4).c_str())-1900;
	
	return mktime(&tp);
}

int parse_common_line(string *line, struct line_data *data) {
	string word;
	int pos;
	unsigned int prev_pos, element;
	
	prev_pos 	= 0;
	element 	= 0;
	pos 		= 0;
	while(element < 4) {
		pos = line->find(" ", prev_pos);
		
		if (pos < 0) return -1;
		
		word = line->substr(prev_pos, pos-prev_pos);
		
		switch(element) {
			case 0: data->sent   = atoi(word.c_str()); break;
			case 1: data->rcvd   = atoi(word.c_str()); break;
			case 2: data->domain.assign(word.c_str()); break;
			case 3:
				pos = line->find(" ", pos+1);
				word = line->substr(prev_pos, pos-prev_pos);
				data->time = parse_common_time(&word);
				break;
		}
		
		element++;
		prev_pos = pos + 1;
		word.clear();
	}
	
	data->line = line->substr(prev_pos, line->length()-prev_pos);
	
	return 0;
}

int parse_error_line(string *line, struct line_data *data) {
	stringstream linestream;
	string word;
	int pos;
	
	if (line->compare(0, 1, "[") == 0 && line->compare(25, 1, "]") == 0) {
		word = line->substr(1,24);
		
		data->time = parse_error_time(&word);
		
		pos = line->find("]", 28);
		if (pos < 0) return -1;
		
		data->domain.assign(line->substr(28, pos-28));
		data->line.assign(line->c_str());
		data->line.erase(27, data->domain.length()+2);
		
		data->sent	= 0;
		data->rcvd	= 0;
		return 0;
	}
	return -2;
}

void do_logrotate() {
	/*	First we close all open log handles	*/
	map<std::string, struct domain_data>::iterator iter;
	
	for(iter=avail_domains.begin(); iter!=avail_domains.end(); ++iter) {
		/*	Currently we never rotate the default log	*/
		if (iter->second.domainname == "default") continue;
		
		if (iter->second.handles.handle != NULL) {
			iter->second.handles.handle->close();
			
			delete iter->second.handles.handle;
			iter->second.handles.handle = NULL;
		}
		
		if (iter->second.handles.user_handle != NULL) {
			iter->second.handles.user_handle->close();
			
			delete iter->second.handles.user_handle;
			iter->second.handles.user_handle = NULL;
			
			/* We also need to remove user's logfile, because we need it's location
			 * No data should be lost, because logrotate uses the non-client location
			 * for creating archives and statistics */
			if (iter->second.handles.user_logfile != NULL && iter->second.handles.user_logfile->length() > 0) {
				if (unlink(iter->second.handles.user_logfile->c_str()) != 0) {
					cerr << "dp-ap-logger: failed to delete user logfile " << iter->second.handles.user_logfile << endl;
				}
				
				delete iter->second.handles.user_logfile;
				iter->second.handles.user_logfile = NULL;
			}
		}
		
		if (iter->second.handles.logfile != NULL) {
			delete iter->second.handles.logfile;
			iter->second.handles.logfile = NULL;
		}
	}
	
	/* Clear bandwidth cache as we need to create new bandwidth structs */
	bw_cache.clear();
	
	if (logrotate_cmd.length() == 0) return;
	
	if (logrotate_stream != NULL) {
		cerr << "dp-ap-logger: unable to start logrotate - last process still running" << endl;
		return;
	}
	
	/*	Try to start log rotation */
	cout << "dp-ap-logger: starting logrotate" << endl;
	
	logrotate_stream = new redi::ipstream();
	logrotate_stream->open(logrotate_cmd, redi::pstreams::pstderr);
	
	if (logrotate_stream->is_open()) {
		logrotate_running = true;
	}
	else {
		cerr << "dp-ap-logger: failed to start logrotate - command failed" << endl;
	}
	
	cout << "dp-ap-logger: log rotation started successfully" << endl;
}

void logrotate_finished(int sig) {
	string line;
	
	if (logrotate_running) {
		cout << "dp-ap-logger: logrotation has finished, output:" << endl;
		
		while(!logrotate_stream->eof()) {
			getline(*logrotate_stream, line);
			
			if (line.length() > 0) cout << "dp-log-rotate: " << line << endl;
		}
		
		logrotate_stream->close();
		
		logrotate_running = false;
		delete logrotate_stream;
		logrotate_stream = NULL;
	}
}

void run() {
	string line;
	struct bandwidth_data bw_data;
	struct line_data data;
	unsigned int current_date = 0;
	int result = 0;
	struct domain_data *d_data;

	init_months();
	collect_domains();
	
	logrotate_running = false;
	signal(SIGCHLD, logrotate_finished);
	
	logrotate_stream = NULL;
	
	for(;;) {
		wait_input();
		
		if (cin.fail() || !cin.good()) {
		    cerr << "dp-ap-logger: input pipe seems to be broken - exiting" << endl;
		    return;
		}
		
		getline(cin, line);
		
		if (line.length() > 0) {
#ifdef DEBUG_ENABLED
			cout << line << endl;
#endif
			
			if (log_type == "error") result = parse_error_line (&line, &data);
			else 					 result = parse_common_line(&line, &data);
			
			if (result < 0) {
				data.domain = "default";
				data.line.assign(line.c_str());
			}
			
			if (avail_domains.count(data.domain) == 0) {
				cerr << "dp-ap-logger: lookup failed for domain " << data.domain << endl;
				data.domain = "default";
				data.line.assign(line.c_str());
			}

			if (avail_domains.count(data.domain) == 0) {
				cerr << "dp-ap-logger: DEFAULT lookup not working!" << endl;
				continue;
			}
			
			d_data 				= &avail_domains[data.domain];
			bw_data.domain_id   = d_data->domain_id;
			bw_data.id   		= 0;
			bw_data.sent 		= data.sent;
			bw_data.rcvd 		= data.rcvd;
			bw_data.time 		= 0;
			bw_data.date 		= get_current_date();
			
			if (result == 0 && bw_data.date != current_date) {
				/*	Date seems different, need to do some checking	*/
				
				cout << "dp-ap-logger: date is now " << bw_data.date << endl;
				
				/*	We do logrotate only if we really detected a date change
				 *  and if we are recording error log, then we do logrotate once per month
				 */
				if (current_date > 0 && (log_type != "error" || bw_data.date-current_date > 31)) {
					/*	Date has really changed, start logrotate	*/
					do_logrotate();
				}
				
				current_date = bw_data.date;
			}
			
			write_log(&data, d_data);
			
#ifdef DEBUG_ENABLED
			cout << "domain=" << data.domain << " sent=" << bw_data.sent <<
					" uid=" << d_data->uid << " gid=" << d_data->gid <<
					" rcvd=" << bw_data.rcvd << " date=" << bw_data.date << endl;
#endif
			if (log_type != "error") update_domain_bandwidth(&bw_data);
		}
	}

	return;
}

void init_mysql() {
	conn.set_option(new mysqlpp::ReconnectOption(true));
	conn.set_option(new mysqlpp::ConnectTimeoutOption(MYSQL_CONNECT_TIMEOUT));
	conn.set_option(new mysqlpp::ReadTimeoutOption(MYSQL_READ_TIMEOUT));
	conn.set_option(new mysqlpp::WriteTimeoutOption(MYSQL_WRITE_TIMEOUT));
	
	conn.enable_exceptions();
	
	try {
	    conn.connect(
					config.read<string>("mysql_database").c_str(),
					config.read<string>("mysql_hostname").c_str(),
					config.read<string>("mysql_username").c_str(),
					config.read<string>("mysql_password").c_str()
	    );
	}
	catch(exception& e) {
	    cerr << "dp-ap-logger: MySQL error: " << e.what() << endl;	    
	}
	
	table_prefix = config.read<string>("table_prefix");
	
	log_root 		= config.read<string>("log_root");
	log_type 		= config.read<string>("log_type");
	logrotate_cmd 	= config.read<string>("logrotate_cmd", "");
	server_id		= config.read<int>("server_id");
	default_logfile	= config.read<string>("default_logfile");
	
	istringstream s(config.read<string>("userlog_perm"));
	s >> oct >> userlog_perm;
}

void init_config(int argc, char *argv[]) {
	string config_file;
	ifstream in;
	
	if (argc == 2) {
		config_file.assign(argv[1]);
	}
	else {
		config_file.assign(DEFAULT_CONFIG_FILE);
	}
	
	in.open(config_file.c_str());
	
	if (!in) throw ConfigFile::file_not_found(config_file);
	
	in >> config;
}

int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
	
	try {
		init_config(argc, argv);
		init_mysql();
		run();
		
		return EXIT_SUCCESS;
	}
	catch(exception& e) {
		cerr << "dp-ap-logger: fatal error : " << e.what() << endl;
	}
	catch(ConfigFile::key_not_found& e) {
		cerr << "dp-ap-logger: missing configuration parameter `" << e.key << "`" << endl;
	}	
	catch(ConfigFile::file_not_found& e) {
		cerr << "dp-ap-logger: failed to open configuration file `" << e.filename << "`" << endl;
	}	

	return EXIT_FAILURE;	
}

#include <sys/types.h>
#include <sys/socket.h>
#include <math.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <list>
#include "util.h"
#include "conn_work.h"
#include "conn_worker.h"
#include "udp_conn_worker.h"
#include "tcp_conn_worker.h"
#include "memcached_cmd.h"
#include "memdb.h"
#include "config.h"

config conf;
static int conn_cnt;
static conn_work **conn_works;

static void init_conf() {
	conf.db_sample_file = "-";
	conf.db_size = 5000;

	conf.mirror = false;

	conf.vclients = 1;

	conf.load = 100000.0;
	conf.qos = 1.0;
	conf.timeout = 20.0;

	conf.udp = false;
	conf.nagles = false;
	conf.burst_size = 1;
	conf.max_outstanding = 10000;
	conf.default_cmd = mcm_get;
	conf.enumerate_items = false;
	conf.set_miss = false;

	conf.preload = false;
	conf.disturbance = 0.5;
}

// c = a - b
static void counters_subtract(double *a, double *b, double *c) {
	for (int i = 0; i < cwc_end; i++) {
		c[i] = a[i] - b[i];
	}
}

// c = a + b
static void counters_plus(double *a, double *b, double *c) {
	for (int i = 0; i < cwc_end; i++) {
		c[i] = a[i] + b[i];
	}
}

static void sum_counters(double *sums) {

	for (int i = 0; i < cwc_end; i++) {
		sums[i] = 0;
	}

	for (int i = 0; i < conn_cnt; i++) {
		double buf[cwc_end];
		conn_works[i]->get_counters(buf);
		counters_plus(sums, buf, sums);
	}
}

static void print_stats_summary(double *d, double t) {
	printf("qos_ratio %.3f load %.0f send_rate %.0f reply_rate %.0f avg_lat %.3f hit_ratio %.3f set_get_ratio %.3f exr %.0f rxr %.0f ryr %.0f co %.0f",
		d[cwc_good_qos_query] / d[cwc_retired_query] * 100.0,
		conf.load,
		d[cwc_sent_query] / t,
		d[cwc_received_reply] / t,
		d[cwc_latency] / d[cwc_replied_query],
		d[cwc_hit_get_query] / d[cwc_replied_get_query],
		d[cwc_sent_set_query] / d[cwc_sent_get_query],
		d[cwc_expired_query] / t,
		d[cwc_expired_reply0] / t,
		d[cwc_expired_reply1] / t,
		d[cwc_corrupted_reply]);
}

static void print_qlen_summary() {

	double cq_max = 0.0;
	double cq_min = conf.max_outstanding;
	double cq_sum = 0.0;

	for (int i = 0; i < conn_cnt; i++) {

		conn_work *work = conn_works[i];
		double qlen = work->get_counter(cwc_outstanding_query);

		cq_sum += qlen;
		if (qlen > cq_max) cq_max = qlen;
		if (qlen < cq_min) cq_min = qlen;
	}

	printf("os_sum %.0f os_max %.0f os_min %.0f os_avg %.0f", cq_sum, cq_max, cq_min, cq_sum / conn_cnt);
}

static void report(double *deltas, long long usec_duration) {
	double duration = (double) usec_duration / 1000000.0;
	print_stats_summary(deltas, duration);
	printf(" ");
	print_qlen_summary();
	printf("\n");
}

static bool preload_done() {
	for (int i = 0; i < conn_cnt; i++) {
		conn_work *work = conn_works[i];
		double query_done = work->get_counter(cwc_replied_query);
		if (query_done < work->db->get_dbsize()) {
			return false;
		}
	}
	return true;
}

static void do_work_round(const work_round &rd) {
	double inits[cwc_end], olds[cwc_end], news[cwc_end], deltas[cwc_end];
	long long init_tv, old_tv, new_tv;

	sum_counters(inits);
	init_tv = clock_mono_usec();
	for (int i = 0; i < rd.iter_cnt || rd.iter_cnt == 0; i++) {

		sum_counters(olds);
		old_tv = clock_mono_usec();
		sleep(rd.interval);
		sum_counters(news);
		new_tv = clock_mono_usec();

		if (rd.discrete) {
			counters_subtract(news, olds, deltas);
			printf("D: ");
			report(deltas, new_tv - old_tv);
		}
		if (rd.accumulate) {
			counters_subtract(news, inits, deltas);
			printf("A: ");
			report(deltas, new_tv - init_tv);
		}
		fflush(stdout);

		if (conf.preload && preload_done()) {
			printf("===preload finished, break round===\n");
			return;
		}
	}
}

static void do_work() {
	printf("===work started===\n");
	for (int rid = 0; rid < (int) conf.work_rounds.size(); rid++) {
		work_round &rd = conf.work_rounds[rid];
		printf("===round %d started[iteratrions=%d, interval=%d]===\n", rid, rd.iter_cnt, rd.interval);
		do_work_round(rd);
		printf("===round %d finished===\n", rid);
	}
	printf("===work finished===\n");
}

static int parse_server_spec(int argc, char **argv) {

	server_record sr;
	int i = 0;

	for (i = 0; i < argc; i += 2) {
		if (argv[i][0] == '-') break;
		server_addr sa;
		sa.hostname = argv[i];
		sa.port = argv[i+1];
		sr.addrs.push_back(sa);
	}

	conf.servers.push_back(sr);
	return i;
}

static int parse_command_spec(int argc, char **argv) {
	int i = 0;
	for (i = 0; i < argc; i++) {
		const char *s = argv[i];
		if (strcmp(s, "set") == 0) {
			conf.default_cmd = mcm_set;
		} else if (strcmp(s, "enum") == 0) {
			conf.enumerate_items = true;
		} else if (strcmp(s, "set-miss") == 0) {
			conf.set_miss = true;
		} else if (s[0] == '-') {
			break;
		} else {
			fprintf(stderr, "parse_command_spec: unknown token: %s\n", s);
			exit(1);
		}
	}
	return i;
}

static int parse_round_spec(int argc, char **argv) {
	work_round rd;
	int i = 0;

	rd.discrete = true;
	rd.accumulate = false;

	if (strcmp(argv[i], "acc") == 0) {
		rd.discrete = false;
		rd.accumulate = true;
		i++;
	} else if (strcmp(argv[i], "all") == 0) {
		rd.discrete = true;
		rd.accumulate = true;
		i++;
	}

	rd.iter_cnt = atof(argv[i++]);
	rd.interval = atof(argv[i++]);

	conf.work_rounds.push_back(rd);

	return i;
}

static void parse_arguments(int argc, char **argv) {

	if (argc == 0) return;

	for (int i = 0; i < argc; ) {
		const char *key = argv[i++];
		if (strcmp(key, "--db") == 0) {
			conf.db_sample_file = argv[i++];
			conf.db_size = atof(argv[i++]);
		} else if (strcmp(key, "--server") == 0) {
			i += parse_server_spec(argc - i, argv + i);
		} else if (strcmp(key, "--mirror") == 0) {
			conf.mirror = true;
		} else if (strcmp(key, "--vclients") == 0) {
			conf.vclients = atof(argv[i++]);
		} else if (strcmp(key, "--load") == 0) {
			conf.load = atof(argv[i++]);
		} else if (strcmp(key, "--qos") == 0) {
			conf.qos = atof(argv[i++]);
		} else if (strcmp(key, "--timeout") == 0) {
			conf.timeout = atof(argv[i++]);
		} else if (strcmp(key, "--round") == 0) {
			i += parse_round_spec(argc - i, argv + i);
		} else if (strcmp(key, "--udp") == 0) {
			conf.udp = true;
		} else if (strcmp(key, "--nagles") == 0) {
			conf.nagles = true;
		} else if (strcmp(key, "--burst-size") == 0) {
			conf.burst_size = atof(argv[i++]);
		} else if (strcmp(key, "--max-outstanding") == 0) {
			conf.max_outstanding = atof(argv[i++]);
		} else if (strcmp(key, "--command") == 0) {
			i += parse_command_spec(argc - i, argv + i);
		} else if (strcmp(key, "--preload") == 0) {
			conf.preload = true;
		} else if (strcmp(key, "--disturbance") == 0) {
			conf.disturbance = atof(argv[i++]);
		} else {
			fprintf(stderr, "parse_arguments: unknown key: %s\n", key);
			exit(1);
		}
	}

	if (conf.preload) {
		conf.load = 0.0; // infinite load
		conf.max_outstanding = 100; // don't overload server
		conf.udp = false;
		conf.default_cmd = mcm_set;
		conf.enumerate_items = true;
		if (conf.mirror) {
			conf.vclients = conf.servers.size();
		} else { // shard
			conf.vclients = 1;
		}
		conf.work_rounds.clear();
	}

	if (conf.work_rounds.size() == 0) {
		work_round rd;
		rd.discrete = true;
		rd.accumulate = false;
		rd.iter_cnt = 0;
		rd.interval = 5;
		conf.work_rounds.push_back(rd);
	}

	if (conf.mirror) {
		if (conf.vclients % conf.servers.size() != 0) {
			fprintf(stderr, "can't divide clients evenly to mirror-servers\n");
			exit(1);
		}
	} else {
		if (conf.db_size % conf.servers.size() != 0) {
			fprintf(stderr, "can't divide db evenly to shard-servers\n");
			exit(1);
		}
	}
}

static server_addr pick_saddr(server_record *srec) {
	std::list<server_addr> *addrs = &srec->addrs;
	addrs->push_back(addrs->front());
	addrs->pop_front();
	return addrs->front();
}

int main(int argc, char **argv) {

	init_conf();

	parse_arguments(argc - 1, argv + 1);

	if (conf.mirror) {
		conn_cnt = conf.vclients;
	} else { // shard
		conn_cnt = conf.vclients * conf.servers.size();
	}

	memdb_sample *sample = new memdb_sample(conf.db_sample_file);
	if (conf.mirror) {
		memdb *db = new memdb(sample, conf.db_size, 0);
		for (int i = 0; i < (int) conf.servers.size(); i++) {
			conf.servers[i].db = db;
		}
	} else { // shard
		int shard_size = conf.db_size / conf.servers.size();
		for (int i = 0; i < (int) conf.servers.size(); i++) {
			conf.servers[i].db = new memdb(sample, shard_size, shard_size * i);
		}
	}

	conn_works = new conn_work*[conn_cnt];
	double avg_load = conf.load / (double) conn_cnt;
	long long seed = clock_mono_usec();
	for (int cid = 0; cid < conn_cnt; cid++) {
		int sid = cid % conf.servers.size();
		server_record *sr = &conf.servers[sid];
		conn_work *work = new conn_work(sr->db, pick_saddr(sr), avg_load, (seed + cid) % RAND_MAX);
		conn_works[cid] = work;
	}

	int cpu_cnt = get_vcpu_cnt(pthread_self());
	cpu_cnt = conn_cnt < cpu_cnt ? conn_cnt : cpu_cnt;
	if (conn_cnt % cpu_cnt != 0) {
		fprintf(stderr, "the number of connections is not a multiple of the number of cpus: %d, %d\n", conn_cnt, cpu_cnt);
		exit(1);
	}

	std::list<conn_work*> *cpu_work_lists = new std::list<conn_work*>[cpu_cnt];
	for (int cid = 0; cid < conn_cnt; cid++) {
		int cpu = cid % cpu_cnt;
		cpu_work_lists[cpu].push_back(conn_works[cid]);
	}
	for (int cpu = 0; cpu < cpu_cnt; cpu++) {
		conn_worker *worker;
		if (conf.udp) {
			worker = new udp_conn_worker(cpu_work_lists[cpu]);
		} else {
			worker = new tcp_conn_worker(cpu_work_lists[cpu]);
		}
		worker->start_thread(cpu);
	}
	delete[] cpu_work_lists;

	fflush(stdout);
	sleep(1); // sleep so that thread initializing messages don't interleave with progress messages

	do_work();
	fflush(stdout);

	return 0;
}

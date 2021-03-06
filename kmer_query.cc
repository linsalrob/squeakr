/*
 * =====================================================================================
 *
 *       Filename:  kmer_query.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/27/2016 08:48:26 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prashant Pandey (), ppandey@cs.stonybrook.edu
 *   Organization:  Stony Brook University
 *
 * =====================================================================================
 */
#include <iostream>
#include <algorithm>
#include <cstring>
#include <vector>
#include <set>
#include <bitset>
#include <cassert>
#include <fstream>

#include <boost/thread/thread.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/atomic.hpp>

#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "threadsafe-gqf/gqf.h"
#include "hashutil.h"
#include "chunk.h"
#include "kmer.h"

#include <zlib.h>
#include <bzlib.h>

#define BITMASK(nbits) ((nbits) == 64 ? 0xffffffffffffffff : (1ULL << (nbits)) - 1ULL)

using namespace std;
using namespace kmercounting;

/* Print elapsed time using the start and end timeval */
void print_time_elapsed(string desc, struct timeval* start, struct timeval* end) 
{
	struct timeval elapsed;
	if (start->tv_usec > end->tv_usec) {
		end->tv_usec += 1000000;
		end->tv_sec--;
	}
	elapsed.tv_usec = end->tv_usec - start->tv_usec;
	elapsed.tv_sec = end->tv_sec - start->tv_sec;
	float time_elapsed = (elapsed.tv_sec * 1000000 + elapsed.tv_usec)/1000000.f;
	cout << desc << "Total Time Elapsed: " << to_string(time_elapsed) << " seconds" << endl;
}

// A=1, C=0, T=2, G=3
void getRandomKmers(int n, uint64_t range, uint32_t seed, vector<uint64_t>& kmers, uint32_t K)
{
	uint64_t kmer;
	for (int j = 0; j < n; j++) {
		kmer = 0;
		for (uint i = 0; i < K; i++) {
			uint8_t c = rand()%4;
			kmer = kmer | c;
			kmer = kmer << 2;
		}
		kmer = kmer >> 2;
		kmer = HashUtil::MurmurHash64A(((void*)&kmer), sizeof(kmer), seed);
		kmers.push_back(kmer%range);
	}
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  main
 *  Description:  
 * =====================================================================================
 */
int main ( int argc, char *argv[] )
{
	QF cf;
	QFi cfi;

	if (argc == 2) {
		string arg_help(argv[1]);
		if (arg_help.compare("-h") != 0 || arg_help.compare("-help") != 0) {
			cout << "./squeakr-query [OPTIONS]" << endl
				   << "file          : dataset Squeakr representation" << endl
					 << "num of queries: number of queries" << endl
					 << "random        : 0 - query for existing k-mers, 1 - query for random k-mers" << endl;
			exit(0);
		}
	}

	string ds_file = argv[1];
	int ksize = atoi(argv[2]);
	uint32_t num_query = atoi(argv[3]);
	int random = atoi(argv[4]);
	struct timeval start, end;
	struct timezone tzp;
	vector<uint64_t> kmers;

	srand(time(NULL));

	//Initialize the QF
	cout << "Reading kmers into the QF off the disk" << endl;
	qf_deserialize(&cf, ds_file.c_str());

	if (random) {
		getRandomKmers(num_query, cf.metadata->range, cf.metadata->seed, kmers, ksize);
	} else {
		uint64_t i = 0;
		qf_iterator(&cf, &cfi, 0);
		do {
			uint64_t key = 0, value = 0, count = 0;
			qfi_get(&cfi, &key, &value, &count);
			i++;
			kmers.push_back(key);
			//freq_file << key << " " << count << endl;
		} while (!qfi_next(&cfi));
		cout << "Total kmers: " << i << endl;
	}

	cout << "Querying kmers in the QF" << endl;
	random_shuffle ( kmers.begin(), kmers.end() );
	uint64_t num_not_found = 0;
	gettimeofday(&start, &tzp);
	for (uint32_t i = 0; i < num_query || i < kmers.size(); i++) {
		/*cout << "index: " << id << endl;*/
		if (!qf_count_key_value(&cf, kmers[i],0)) {
			num_not_found++;
			//cout << "Can not find the kmer: " << kmers[id] << endl;
			//abort();
		}
	}
	gettimeofday(&end, &tzp);
	print_time_elapsed("", &start, &end);
	
	cout << "Not find: " << num_not_found << endl;

	return EXIT_SUCCESS;
}				/* ----------  end of function main  ---------- */



/* Add appropriate header files */

struct config {
    int capacity;
    int readers;
    int writers;
    int writer_batch;
    int seconds;
    int rd_us;
    int wr_us;
    int dump_csv;
};

static void print_usage(const char *progname) 
{
	fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "-c,  --capacity <N>        Log capacity (default 1024)\n"
        "-r,  --readers <N>         Number of reader threads (default 6)\n"
        "-w,  --writers <N>         Number of writer threads (default 4)\n"
        "-b,  --writer-batch <N>    Entries written per writer section (default 2)\n"
        "-s,  --seconds <N>         Total run time (default 10)\n"
        "-R,  --rd-us <usec>        Reader sleep between operations (default 2000)\n"
        "-W,  --wr-us <usec>        Writer sleep between operations (default 3000)\n"
        "-d,  --dump                Dump final log to log.csv\n"
        "-h,  --help                Show this help message\n",
        progname);
}

static void parse_args(int argc, char **argv, struct config *cfg) 
{
	// Set defaults
    cfg->capacity     = 1024;
    cfg->readers      = 6;
    cfg->writers      = 4;
    cfg->writer_batch = 2;
    cfg->seconds      = 10;
    cfg->rd_us        = 2000;
    cfg->wr_us        = 3000;
    cfg->dump_csv     = 0;
	
	/* define the long_opts options */
	
	
	/* Parse each of the arguments using getopt_long() function */
	
}

int main(int argc, char **argv) 
{
    struct config cfg;
    parse_args(argc, argv, &cfg);

    printf("capacity=%d readers=%d writers=%d batch=%d seconds=%d rd_us=%d wr_us=%d seed=%d dump=%d\n",
           cfg.capacity, cfg.readers, cfg.writers, cfg.writer_batch,
           cfg.seconds, cfg.rd_us, cfg.wr_us, cfg.seed, cfg.dump_csv);

    /* your remaining initialization here... */
	
	
	/* Initialize the shm-backed monitor */
 
 
    /* Install SIGINT and start wall-clock timer thread */
	
	
	 /* Create the writer threads */
	 
	 
	 /* Create the reader threads */
	 
	 
	 /* Join reader/writer threads and timer thread */
	 
	 
	 /* Optional: dump the final log to CSV for inspection/grading. */
	 
	 
	 /* Compute averages only (avg reader wait, avg writer wait, avg throughput) */
	 
	 
	  /* Cleanup heap and monitor resources */
	  
	  
	  return 0;
	  
}
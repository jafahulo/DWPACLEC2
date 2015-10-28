#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include  <signal.h>
#include <sqlite3.h>
#include <iostream>

#include "headers/common.h"
#include "headers/cpu-crack.h"
#include "headers/gpu-crack.h"

#include <iostream>
using std::cout;
using std::endl;

#include <fstream>
using std::ifstream;

#include <cstring>

const int MAX_CHARS_PER_LINE = 25;
const int MIN_CHARS_PER_LINE = 8;
using namespace std;
//in case there are 8 cpu threads + 1 for 1 GPU
//global db connector
MYSQL* MySQLConnection[NUM_DB_CONNECTIONS];
unsigned long keys;
int vflag;
// read an entire line into memory
char buf[MAX_CHARS_PER_LINE];
int verbose=1;
int read_from_file(char* name_of_file,sqlite3_stmt* stmt)
{
	//the number of words we have skipped due to length.
	int skipped = 0;
	//the number of words greater than 7 and less than 63
	int wordsAdded = 0;
	// create a file-reading object
	ifstream fin;
	fin.open(name_of_file); // open a file
	if (!fin.good())
	{
		fprintf(stderr,"File not found\n");
		return wordsAdded;
	}
	// read each line of the file
	while (!fin.eof())
	{
		fin.getline(buf,12);
		int length = strnlen(buf,12);

		/* Test length of word.  IEEE 802.11i indicates the passphrase must be
		 * at least 8 characters in length, and no more than 63 characters in
		 * length.
		 */
		if (length < MIN_CHARS_PER_LINE || length > MAX_CHARS_PER_LINE) {
			if (verbose) {
				fprintf(stderr, "Invalid passphrase length: %s (%d).\n",buf, length);
				fprintf(stderr, "Skipped a word\n");
			}
			/*
			 *                           * Output message to user*/
			skipped++;
			//continue;
		} else {
			/* This word is good, increment the words tested counter */
			wordsAdded++;

			sqlite3_bind_text(stmt, 1, buf, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 2, length);
			sqlite3_step(stmt);     /* Execute the SQL Statement */
			sqlite3_clear_bindings(stmt);   /* Clear bindings */
			sqlite3_reset(stmt);        /* Reset VDBE */
			if(verbose)
			{
				printf("Length: %d\tWord: %s\n",length,buf);
			}
		}
	}
	printf("Skipped %d words due to length\n",skipped);
	printf("Added %d words to the database\n",wordsAdded);
	return wordsAdded;
}
int open_lite_db(char* output_file, char* input_file)
{

	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	int BUFFER_SIZE = 100;
	char sSQL [BUFFER_SIZE];
	sqlite3_stmt *createStmt;
	sqlite3_stmt* stmt;
	rc = sqlite3_open(output_file, &db);
	if(rc){
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return(1);
	}

	string createQuery = "CREATE TABLE IF NOT EXISTS DICT1 (WORD CHAR(20) NOT NULL UNIQUE PRIMARY KEY,LENGTH TINYINT UNSIGNED NOT NULL,CHECK (LENGTH>7));";
	cout << "Creating Table Statement" << endl;
	sqlite3_prepare(db, createQuery.c_str(), createQuery.size(), &createStmt, NULL);
	cout << "Stepping Table Statement" << endl;
	if (sqlite3_step(createStmt) != SQLITE_DONE) cout << "Didn't Create Table!" << endl;

	sqlite3_exec(db, "PRAGMA synchronous = OFF", NULL, NULL, &zErrMsg);
	sqlite3_exec(db, "PRAGMA journal_mode = MEMORY", NULL, NULL, &zErrMsg);
	//sSQL = "\0";
	sprintf(sSQL, "INSERT INTO DICT1(WORD,LENGTH) VALUES (@buf, @length)");
	sqlite3_prepare(db,  sSQL, BUFFER_SIZE, &stmt, NULL);
	sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &zErrMsg);
	return read_from_file(input_file,stmt);

}
int handle_db_connect(int c)
{

	printf("What would you like to do?\n");
	printf("1 - Retry\n2 - read passwords from a file\n3 - Specific different database\n4 - Quit\n" );
	c = getchar();
	getchar();
	if(c=='1')
	{
		return 1;
	}
	if(c=='2')
	{
		char input_filename[30];
		char output_filename[30];
		printf("File must have 1 password per line\n");
		printf("Name of file to read from: \n");
		if(read(STDIN_FILENO,input_filename,25)<=0)
		{
			fprintf(stderr,"You messed up\n");
		}
		printf("Name of file to store sqlite DB: \n");

		if(read(STDIN_FILENO,output_filename,25)<=0)
		{
			fprintf(stderr,"You messed up\n");
		}
		if(open_lite_db(output_filename,input_filename)!=0)
		{
			printf("Sucess\n");
		}
		else
		{
			printf("Fail\n");
		}
		return 0;

	}
	else if (c=='3')
	{
		printf("Not yet supported\n");
		exit(1);
	}
	else if ( c=='4')
	{
		exit(1);

	}
	return 0;
}
int connect_to_db(int id,char* hostName)
{
	// --------------------------------------------------------------------
	//     // Connect to the database

	MySQLConnection[id] = mysql_init( NULL );

	//read user input
	int c;

	//timeout after this many seconds of trying to connect
	int timeout = 30;

	mysql_options(MySQLConnection[id], MYSQL_OPT_CONNECT_TIMEOUT, (const char *)&timeout);
	//do
	//{
	//printf("HostName / DB_IP is %s\n",hostName);
	//printf("Quitting\n");
	//return(1);
	hostName = "mydbinstance1.cs5euu09kkcz.us-east-1.rds.amazonaws.com";
	if(!mysql_real_connect(MySQLConnection[id], // MySQL obeject
				hostName, // Server Host
				userId,// User name of user
				password,// Password of the database
				DB_NAME,// Database name
				PORT_NUMBER,// port number
				NULL,// Unix socket  ( for us it is Null )
				0))
	{
		printf("Error %u: %s\n", mysql_errno(MySQLConnection[id]), mysql_error(MySQLConnection[id]));
		printf("proceed anyway\n\n");
		//c='y';
		//printf("Proceed anyway? (y | n)");
		c = getchar();
		getchar();
		if(c=='n')
			return(1);
		//else if(c=='y')
		// {
		//   c=handle_db_connect(c);
	}
	//}
	//	 throw FFError( (char*) mysql_error(MySQLConnection[id]) );
	//}while(c=='y');
	///  printf("MySQL Connection Info: %s \n", mysql_get_host_info(MySQLConnection));
	//printf("MySQL Client Info: %s \n", mysql_get_client_info());
	//printf("MySQL Server Info: %s \n", mysql_get_server_info(MySQLConnection));

	return 0;
}


int wait_connect(int* psd,int port)
{
	int sd = -1, rc = -1, on = 1;
	struct sockaddr_in serveraddr, their_addr;

	// get a socket descriptor
	if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	// allow socket descriptor to be reusable
	if((rc = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on))) < 0)
	{
		close(sd);
		return -1;
	}

	// bind to an address
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if((rc = bind(sd, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) < 0)
	{
		close(sd);
		return -1;
	}

	// up to 1 client can be queued
	if((rc = listen(sd, 1)) < 0)
	{
		close(sd);
		return -1;
	}

	// accept the incoming connection request
	socklen_t sin_size = sizeof(struct sockaddr_in);
	if((*psd = accept(sd, (struct sockaddr*)&their_addr, &sin_size)) < 0)
	{
		close(sd);
		return -1;
	}

	return 0;
}

int master_request(int sd, unsigned long* pfpwd, unsigned long* plpwd, wpa_hdsk* phdsk, char* essid,char* DB_IP)
{
	char buf[1024];
	char buf2[32];
	char essid_len = 0;
	int count = 0, rc = 0;
	int tcnt = 17+sizeof(wpa_hdsk);

	count = write(sd, &("r"), 1);
	if(count != 1)
		return -1;

	count = 0;
	memset(buf, 0, 512);
	while (count < tcnt)
	{
		rc = read(sd, &buf[count], 512-count);

		if (rc <= 0)
			return -1;
		else
			count += rc;
	}

	memset(buf2, 0, 32);
	//copy range start
	memcpy(buf2, buf, 8);
	*pfpwd = atol(buf2);
	memset(buf2, 0, 32);
	//copy range end
	memcpy(buf2, &buf[8], 8);
	*plpwd = atol(buf2);


	memcpy(phdsk, &buf[16], sizeof(wpa_hdsk));
	essid_len = buf[16+sizeof(wpa_hdsk)];
	while (count < tcnt+essid_len)
	{
		rc = read(sd, &buf[count], 512-count);

		if (count <= 0)
			return -1;
		else
			count += rc;
	}
	memset(essid, 0, 32);
	memcpy(essid, &buf[17+sizeof(wpa_hdsk)], essid_len);

	//copy DB ip address
	memset(DB_IP, 0, 32);
	memcpy(DB_IP, &buf[17+sizeof(wpa_hdsk)+essid_len], 16);
	printf("\nm_DB is : %s\n",DB_IP);
	// reset the key mic field in eapol frame to zero for later calculation
	memset(&(phdsk->eapol[81]), 0, 16);

	return 0;
}

int master_answer(int sd, char* key)
{
	unsigned char len = 0;
	int count = 0;

	len = strlen(key);

	count = write(sd, &("a"), 1);
	if(count != 1)
		return 0;
	count = write(sd, &len, 1);
	if(count != 1)
		return 0;
	count = write(sd, key, len);
	if(count != len)
		return 0;

	return 1;
}

void print_work(unsigned long* pfpwd, unsigned long* plpwd, wpa_hdsk* phdsk, char* essid)
{
	int i = 0;

	printf("----------------------------------------\n");
	printf("password range: %08lu to %08lu\n", *pfpwd, *plpwd);
	printf("essid: %s\n", essid);
	printf("s-mac: %02x", phdsk->smac[0]);
	for (i=1; i<6; ++i)
		printf(":%02x", phdsk->smac[i]);
	putchar('\n');
	printf("a-mac: %02x", phdsk->amac[0]);
	for (i=1; i<6; ++i)
		printf(":%02x", phdsk->amac[i]);
	putchar('\n');
	printf("s-nonce: ");
	for (i=0; i<32; ++i)
		printf("%02x", phdsk->snonce[i]);
	putchar('\n');
	printf("a-nonce: ");
	for (i=0; i<32; ++i)
		printf("%02x", phdsk->anonce[i]);
	putchar('\n');
	printf("key version: %u (%s)\n", phdsk->keyver, phdsk->keyver==1?"HMAC-MD5":"HMAC-SHA1-128");
	printf("key mic: ");
	for (i=0; i<16; ++i)
		printf("%02x", phdsk->keymic[i]);
	putchar('\n');
	printf("eapol frame content size: %u bytes\n", phdsk->eapol_size);
	printf("eapol frame content (with mic reset): \n");
	for (i=1; i<=phdsk->eapol_size; ++i)
		printf("%02x%c", phdsk->eapol[i-1], i%16==0?'\n':' ');
	putchar('\n');
	printf("----------------------------------------\n");
}
void INThandler(int sig)
{
	//char  c;
	//char message[128];
	//  int i;
	signal(sig, SIG_IGN);
	//snprintf(message,128,"OUCH, did you hit Ctrl-C?\nDo you really want to quit? [y/n] ");
	//write(1,message,128);
	//c = getchar();
	//if (c == 'y' || c == 'Y')
	//{
	//int i;
	//'agressive loop optimization' complains about this

	//not sure how to have cpu_num passed in

	// for(i=0;i<=10;i++)
	//{
	mysql_close(MySQLConnection[0]);
	//      }
	exit(0);
	// }
	//getchar(); // Get new line character
}
int main(int argc, char** argv)
{
	int cpu_num = 0;
	int cpu_working = 0;
	int gpu_num = 0;
	int gpu_working = 0;
	int sd = -1;
	int flag = -1;
	int i = 0;
	keys=0;
	wpa_hdsk hdsk;
	char essid[32];
	/**DB IP Adress */
	char hostName[256];
	pwd_range range;
	unsigned long first_pwd = 0;
	unsigned long last_pwd = 0;
	//port number to open at
	int port;

	// get the number of CPU processors
	/* cpu_num = sysconf(_SC_NPROCESSORS_ONLN );
	   if(cpu_num > 8)
	   {
	   cpu_num=8;
	   }*/
	cpu_num=0;
	//having each one connect to DB slows things down.
	//lets not have any
	//cpu_num=0;	   
	printf("number of CPU processors: %d\n", cpu_num);
	//for debugging
	//gpu_num=1;  
	gpu_num = num_of_gpus();
	printf("number of GPU devices: %d\n", gpu_num);

	// check and parse arguments
	if (argc > 3)
	{
		fprintf(stderr,"Too many args\n");
		fprintf(stderr,"usage: %s <port>\n", argv[0]);
		exit(0);
	}
	else
	{
		if(argv[1]==NULL)
		{
			printf("Sunjay has opened port# 7373 for this\n");
			printf("Enter a port number\n");
			scanf("%d", &port);
		}
		else
		{
			port=atoi(argv[1]);
		}
		if(argv[2]!=NULL)
		{

			if(strcmp(argv[2],"-v")==0)
			{
				vflag=1;
				printf("Verbose flag is set\n");
			}
			else
			{
				printf("usage: %s <port>\n", argv[0]);
				exit(0);

			}
		}
	}
	//printf("Testing connection to database\n");

	//   db_flag=1;
	/* if(connect_to_db(0)==0)
	   {
	   printf("DB_connector_index: %d Connecting to DB: ",0);
	   printf("Successful\n");
	   db_flag=1;
	// printf("MySQL Connection Info: %s \n", mysql_get_host_info(MySQLConnection[i]));
	}
	else
	{
	printf("Connecting to DB: ");
	printf("fail\n");
	printf("Will not attempt to connect to %s DB again\n",DB_NAME);
	db_flag=0;
	return 0;
	}*/

	// connect to the master
	printf("wait for connection from master on port %d ...\n", port);
	flag = wait_connect(&sd,port);
	printf("%s\n", flag==0?"connection established":"failed");
	if (flag)
		exit(0);

	// request for work from the master
	printf("requesting for work from the master ... ");
	flag = master_request(sd, &first_pwd, &last_pwd, &hdsk, essid, hostName);
	printf("%s\n", flag==0?"done":"failed");
	if (flag)
	{
		close(sd);
		exit(0);
	}

	// print out the received information
	print_work(&first_pwd, &last_pwd, &hdsk, essid);


	// prepare for CPU and GPU thread creation
	range = fetch_pwd('\0', &first_pwd, &last_pwd);
	if ( range.start != 0)
	{
		printf("error while preparing cpu/gpu thread creation\n");
		printf("Range does not start at 0\n");
		exit(0);
	}
	float* calc_speed = (float*)malloc((cpu_num+1)*sizeof(float));
	memset(calc_speed, 0, (cpu_num+1)*sizeof(float));
	char final_key[128];
	memset(final_key, 0, sizeof(final_key));
	char final_key_flag = 0;
	// Number of Host threads = Number of CPU Crack Threads + 1 GPU Managing Thread for all GPUs
	// For GPU, it is wasteful to have a separate host thread for each GPU, since the host thread *could*
	// spin wait for the GPU to finish, wasting precious CPU cycles
	pthread_t* tid_vector = (pthread_t*)malloc((cpu_num+1)*sizeof(pthread_t));
	memset(tid_vector, 0, (cpu_num+1)*sizeof(pthread_t));

	// For calculating the total time taken
	struct timeval tprev;
	struct timeval tnow;

	// Start time of the computation
	gettimeofday ( &tprev , NULL );

	//open db connections for each CPU thread
	for (i=0; i<cpu_num; ++i)
	{
		//if successfully connected to the database, so we can make queries, process
		if(connect_to_db(i,hostName)==0)
		{
			if(vflag)
			{
				printf("DB_connector_index: %d CPU %d: Connecting to DB: ",i,i);
				printf("Successful\n");
			}	 
			// printf("MySQL Connection Info: %s \n", mysql_get_host_info(MySQLConnection[i]));
		}
		else
		{
			printf("Connecting to DB: ");
			printf("fail\n");
			exit(0);
		}
	}
	// create the cracking threads for CPU
	for (i=0; i<cpu_num; ++i)
	{
		ck_td_struct* arg = (ck_td_struct*)malloc(sizeof(ck_td_struct));
		arg->cpu_core_id = i;
		arg->gpu_core_id = -1;
		arg->set_affinity = 1;
		arg->essid = essid;
		arg->phdsk = &hdsk;
		arg->calc_speed = calc_speed;
		arg->final_key = final_key;
		arg->final_key_flag = &final_key_flag;


		flag = pthread_create(&tid_vector[i], NULL, crack_cpu_thread, arg);
		if (flag)
		{
			printf("Can't create thread on cpu core %d: %s\n", i, strerror(flag));
		}
		else
		{
			if(vflag)
			{
				printf("Created CPU %d thread\n",i);
			}
			cpu_working++;
		}
	}

	// create the cracking host thread for GPU
	// the host thread will dispatch work to all available GPU devices
	ck_td_struct* arg = (ck_td_struct*)malloc(sizeof(ck_td_struct));
	arg->cpu_core_id = cpu_num;
	arg->gpu_core_id = gpu_num; // Num of GPUs available
	arg->set_affinity = 1;
	arg->essid = essid;
	arg->phdsk = &hdsk;
	arg->calc_speed = calc_speed;
	arg->final_key = final_key;
	arg->final_key_flag = &final_key_flag;

	//open db connections for each GPU thread
	for (i=0; i<gpu_num; ++i)
	{
		if(vflag==1)
		{
			printf("testing connection to db for gpu at connection id %d\n",cpu_num+i);
		}
		//if successfully connected to the database, so we can make queries, process
		if(connect_to_db(cpu_num+i,hostName)==0)
		{
			if(vflag)
			{
				printf("DB_connector_index: %d GPU %d: Connecting to DB: ",cpu_num+i,i);
				printf("Successful\n");
			}
			//   printf("MySQL Connection Info: %s \n", mysql_get_host_info(MySQLConnection[i]));
		}
		else
		{
			printf("GPU %d: Cnnecting to DB: ",i);
			printf("fail\n");
			exit(0);
		}
	}
	flag = pthread_create(&tid_vector[cpu_num], NULL, crack_gpu_thread, arg);
	if (flag)
	{
		printf("can't create thread for gpu: %s\n", strerror(flag));
	}
	else
	{
		printf("created thread for gpu\n");
		gpu_working++;
	}
	// monitor the runtime status
	//float cpu_speed_all = 0;
	//float gpu_speed_all = 0;
	//printf ( "...\n" );
	printf("Starting WPS-cack-s loop\n");
	while (1)
	{
		signal(SIGINT, INThandler);
		// print the calculation speed
		//cpu_speed_all = 0;
		//gpu_speed_all = 0;

		for (i=0; i<cpu_num; ++i)
		{
			if (calc_speed[i] == -1)
			{
				calc_speed[i] = -2;
				cpu_working--;
			}
			//else if (calc_speed[i] > 0)
			//cpu_speed_all += calc_speed[i];
		}

		// Speed of all GPUs
		if (calc_speed[cpu_num] == -1)
		{
			calc_speed[cpu_num] = -2;
			gpu_working--;
		}
		//else if (calc_speed[cpu_num] > 0)
		//gpu_speed_all += calc_speed[cpu_num];

		// delete the last output line (http://stackoverflow.com/questions/1348563)
		//fputs("\033[A\033[2K",stdout);
		//rewind(stdout);
		//flag = ftruncate(1, 0);

		//range = fetch_pwd('\0', &first_pwd, NULL);
		//printf("SPD: %08.1fPMK/S [CPU(%02d):%08.1f|GPU(%02d):%08.1f|G/C:%06.1f] CUR: %08lu\n",cpu_speed_all+gpu_speed_all, cpu_working, cpu_speed_all, gpu_working, gpu_speed_all, gpu_speed_all/cpu_speed_all, range.start);

		// reset for some time (this only blocks the current thread)
		// this seems unnecessary..
		// sleep(1);

		// check if the correct key is found
		if (final_key_flag)
		{
			printf("!!! key found !!! [%s]\n", final_key);

			// End time of computation
			gettimeofday ( &tnow , NULL );
			// Report total time
			float total_time = tnow.tv_sec - tprev.tv_sec + ( tnow.tv_usec - tprev.tv_usec ) * 0.000001F;
			printf ( "Time Taken: %.2f seconds, i.e. %.2f hours\n" , total_time , total_time / 3600 );

			printf("Sending the key back to the master ... ");
			flag = master_answer(sd, final_key);
			if (flag)
			{
				printf("done\n");
				close(sd);
				free(calc_speed);
				printf("Final key has been found, quitting\n");
				//mysql_library_end();
				// exit(0);
			}
			else
			{
				printf("failed\n");
				char name_of_file[256];
				fprintf(stderr,"Unable to send key to master\n");
				sprintf(name_of_file,"WPA_PASSWORD");
				fprintf(stderr,"Writing to file (%s)\n",name_of_file);
				FILE *f = fopen(name_of_file, "w");
				if (f == NULL)
				{
					fprintf(stderr,"Error opening %s\n",name_of_file);
					exit(1);
				}
				/* write password to file*/
				fprintf(f, "%s",final_key);
				fclose(f);
				//close(sd);
				// free(calc_speed);
				exit(1);
			}
		}
		// if there are no remaining cpu or gpu thread, then exit
		if (gpu_working<=0)
			//	   if (cpu_working<=0 || gpu_working<=0)
		{
			//printf("no thread calculating exit\n");
			close(sd);
			// free(calc_speed);
			break;
			return 0;   
			//  exit(0);
		}
	}
	printf("Key not found\n");
	// End time of computation
	gettimeofday ( &tnow , NULL );
	// Report total time
	float total_time = tnow.tv_sec - tprev.tv_sec + ( tnow.tv_usec - tprev.tv_usec ) * 0.000001F;
	printf ( "Time Taken: %.2f seconds, i.e. %.2f hours\n" , total_time , total_time / 3600 );
	// release resources
	close(sd);
	free(calc_speed);
	return 0;
}

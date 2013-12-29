//#define _GNU_SOURCE
#include <iostream>
#include <mysql/mysql.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <pthread.h>
#include <sys/time.h>

#include <openssl/hmac.h>
#include <openssl/sha.h>

#include "headers/cpu-crack.h"

#ifndef uchar
#define uchar unsigned char
#endif

#ifndef uint
#define uint unsigned int
#endif

//time stuff
//globals
struct timeval start_time, end_time;
float elapsed_time = 0;

class FFError
{
   public:
      std::string    Label;

      FFError( ) { Label = (char *)"Generic Error"; }
      FFError( char *message ) { Label = message; }
      ~FFError() { }
      inline const char*   GetMessage  ( void )   { return Label.c_str(); }
};
inline void calc_pmk(const char *key,size_t len, char *essid_pre, uchar pmk[40])
{
   int i, j, slen;
   uchar buffer[64];
   char essid[32+4];
   SHA_CTX ctx_ipad;
   SHA_CTX ctx_opad;
   SHA_CTX sha1_ctx;

   memset(essid, 0, sizeof(essid));
   memcpy(essid, essid_pre, strlen(essid_pre));
   slen = strlen(essid)+4;

   /* setup the inner and outer contexts */

   memset(buffer, 0, sizeof(buffer));
   //ORIGINAL
   //memcpy(buffer, key, strlen(key));
   //SUNJAY
   memcpy(buffer, key, len);

   for(i=0; i<64; i++)
      buffer[i] ^= 0x36;

   SHA1_Init( &ctx_ipad );
   SHA1_Update( &ctx_ipad, buffer, 64 );

   for(i=0; i<64; i++)
      buffer[i] ^= 0x6A;

   SHA1_Init(&ctx_opad);
   SHA1_Update(&ctx_opad, buffer, 64);

   /* iterate HMAC-SHA1 over itself <b>8192<b> times */

   essid[slen-1] = '\1';
   //ORIGINAL
   //HMAC(EVP_sha1(), (uchar*)key, strlen(key), (uchar*)essid, slen, pmk, NULL);
   //SUNJAY
   HMAC(EVP_sha1(), (uchar*)key, len, (uchar*)essid, slen, pmk, NULL);
   memcpy(buffer, pmk, 20);

   for(i=1; i<4096; i++)
   {
      memcpy(&sha1_ctx, &ctx_ipad, sizeof(sha1_ctx));
      SHA1_Update(&sha1_ctx, buffer, 20);
      SHA1_Final(buffer, &sha1_ctx);

      memcpy(&sha1_ctx, &ctx_opad, sizeof(sha1_ctx));
      SHA1_Update(&sha1_ctx, buffer, 20);
      SHA1_Final(buffer, &sha1_ctx);

      for(j=0; j<20; j++)
         pmk[j] ^= buffer[j];
   }

   essid[slen-1] = '\2';
   //ORIGINAL
   //HMAC(EVP_sha1(), (uchar*)key, strlen(key), (uchar*)essid, slen, pmk+20, NULL);
   //SUNJAY
   HMAC(EVP_sha1(), (uchar*)key, len, (uchar*)essid, slen, pmk+20, NULL);
   memcpy(buffer, pmk+20, 20);

   for(i=1; i<4096; i++)
   {
      memcpy(&sha1_ctx, &ctx_ipad, sizeof(sha1_ctx));
      SHA1_Update(&sha1_ctx, buffer, 20);
      SHA1_Final(buffer, &sha1_ctx);

      memcpy(&sha1_ctx, &ctx_opad, sizeof(sha1_ctx));
      SHA1_Update(&sha1_ctx, buffer, 20);
      SHA1_Final(buffer, &sha1_ctx);

      for(j=0; j<20; j++)
         pmk[j+20] ^= buffer[j];
   }
}

/* used to fetch password segment */
/* type is 'c' for CPU and 'g' for GPU, first and last should always be NULL  */
pwd_range fetch_pwd(char type, const unsigned long* first, const unsigned long* last)
{
   static pthread_mutex_t mutex;
   static pwd_range range;
   static unsigned long current_self;
   static unsigned long last_self;

   if (first && last) // called by master thread only to do initialization
   {
      current_self = *first;
      last_self = *last;
      if (pthread_mutex_init(&mutex, NULL) != 0)
      {
         range.start = 0.5;
         range.end = 0.5;
      }
   }
   else if (first && !last) // called by master thread only to get current status
   {
      range.start = current_self;
      range.end = 0.5;
   }
   else
   {
      pthread_mutex_lock(&mutex);
      if (current_self > last_self)
      {
         range.start = 0.5;
         range.end = 0.5;
      }
      else
      {
         unsigned long len;
         if (type == 'c')
         {
            len = PWD_BATCH_SIZE_CPU;
         }
         else
         {
            len = PWD_BATCH_SIZE_GPU;
         }
         range.start = current_self;
         range.end = (current_self+len-1>last_self)?last_self:(current_self+len-1);
         current_self += len;
      }
      pthread_mutex_unlock(&mutex);
   }

   return range;
}

void do_time_stuff(int flag)
{
   if(flag==1)
   {
      gettimeofday(&start_time, 0);
   }
   else
   {
      gettimeofday(&end_time, 0);
      /* print time elapsed */
      if (end_time.tv_usec < start_time.tv_usec) {
         end_time.tv_sec -= 1;
         end_time.tv_usec += 1000000;
      }
      end_time.tv_sec -= start_time.tv_sec;
      end_time.tv_usec -= start_time.tv_usec;
      elapsed_time = end_time.tv_sec + end_time.tv_usec / 1000000.00;
      printf("%.3f seconds elapsed for query\n",elapsed_time);
   }
   // printf("\n%d passphrases tested in %.2f seconds:  %.2f passphrases/second\n", i, elapsed, i / elapsed);
}
//the main CPU crack thread that does all the work
void* crack_cpu_thread(void *arg)
{
   char essid[32];
   //char key[128];
  char* key;
   uchar pmk[40];
   uchar pke[100];
   uchar ptk[80];
   uchar mic[20];


   //connection to DB
   MYSQL* MySQLConnection = NULL;
   MYSQL_RES      *mysqlResult = NULL;
   MYSQL_ROW       mysqlRow;
   unsigned int numRows;
   unsigned int numFields;
   char query[50];
   //////

   ck_td_struct* ck_td_arg = (ck_td_struct*)arg;
   wpa_hdsk* phdsk = ck_td_arg->phdsk;
   int cpu_core_id = ck_td_arg->cpu_core_id;
   float* calc_speed = ck_td_arg->calc_speed;
   char* final_key = ck_td_arg->final_key;
   char* final_key_flag = ck_td_arg->final_key_flag;
   memset(essid, 0, sizeof(essid));
   memcpy(essid, ck_td_arg->essid, 32);

   pthread_t tid;
   tid = pthread_self();

   int i = 0;
   unsigned long cnt = 0;
   int cnt_int = 250;
   struct timeval tnow;
   struct timeval tlast;
   int flag = 0;
   pwd_range range;
   unsigned long cur_key_digit;

   // set cpu affinity
   if (ck_td_arg->set_affinity)
   {
      cpu_set_t set;
      CPU_ZERO(&set);
      CPU_SET(cpu_core_id, &set);
      flag = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &set);
      if (flag)
         printf("thread %u set setaffinity failed: %s\n", (unsigned)tid, strerror(flag));
   }

   //if successfully connected to the database, so we can make queries, process
   // if(mysqlConnect(MySQLConnection)==0)

   // --------------------------------------------------------------------
   //     // Connect to the database

   MySQLConnection = mysql_init( NULL );

   try
   {
      if(!mysql_real_connect(MySQLConnection, // MySQL obeject
               hostName, // Server Host
               userId,// User name of user
               password,// Password of the database
               DB_NAME,// Database name
               0,// port number
               NULL,// Unix socket  ( for us it is Null )
               0))
      {
         throw FFError( (char*) mysql_error(MySQLConnection) );
      }
      printf("MySQL Connection Info: %s \n", mysql_get_host_info(MySQLConnection));
      printf("MySQL Client Info: %s \n", mysql_get_client_info());
      printf("MySQL Server Info: %s \n", mysql_get_server_info(MySQLConnection));

   }
   catch (FFError e )
   {
      printf("%s\n",e.Label.c_str());
      return NULL;
   }
   // --------------------------------------------------------------------
   // do the calculation
   //keep going until we find the password
   while (1)
   {
      // get the password range
      range = fetch_pwd('c', NULL, NULL);
      if (range.start ==0.5)
      {
         printf("Range does not start at 0\n");
         printf("CPU thread is exitting\n");
         break;
      }
      // loop through each key in range
      // for (cur_key_digit = range.start; cur_key_digit <= range.end; ++cur_key_digit)

      ////SUNJAY ***************


      // --------------------------------------------------------------------
      // Perform a SQL SELECT and retrieve data
      try
      {
         sprintf(query, "SELECT DICT.WORD FROM DICT LIMIT %d OFFSET %lu;",PWD_BATCH_SIZE_CPU,range.start);
         printf("Query is: %s\n",query);
         if(mysql_query(MySQLConnection,query))
            throw FFError( (char*)mysql_error(MySQLConnection) );
         else
            mysqlResult = mysql_store_result(MySQLConnection); // Get the Result Set

         if (mysqlResult)  // there are rows
         {
            // # of rows in the result set
            numRows = mysql_num_rows(mysqlResult);

            // # of Columns (mFields) in the latest results set
            //       numFields = mysql_field_count(MySQLConnection);

            // Returns the number of columns in a result set specified
            numFields = mysql_num_fields(mysqlResult);

            printf("Number of rows=%u  Number of fields=%u \n",numRows,numFields);
         }
         else
         {
            printf("Result set is empty");
            return NULL;
         }
         //loop through all the rows in the result set
         while(mysqlRow = mysql_fetch_row(mysqlResult)) // row pointer in the result set
         {
            // calculate the calculation speed
            if (cnt == 0)
               gettimeofday(&tlast, NULL);
            else if (cnt%cnt_int == 0)
            {
               gettimeofday(&tnow, NULL);
               calc_speed[cpu_core_id] = 1.0*cnt_int/(tnow.tv_sec-tlast.tv_sec+(tnow.tv_usec-tlast.tv_usec)*0.000001);
               gettimeofday(&tlast, NULL);
            }
            cnt++;

            // reset variables for the new word
            memset(key, 0, sizeof(key));
            memset(pmk, 0, sizeof(pmk));
            memset(pke, 0, sizeof(pke));
            memset(ptk, 0, sizeof(ptk));
            memset(mic, 0, sizeof(mic));

            //ORIGINAL
            // convert the key from digit to string
            //  sprintf(key, "%08lu", cur_key_digit);
            ////SUNJAY ***************



            //printf("CPU Password: %s\n",key);
            // calculate the PMK
            key=mysqlRow[0];
            calc_pmk(key,strlen(key), essid, pmk);
            //SUNJAY
            //calc_pmk("hello", essid, pmk);

            // pre-compute the key expansion buffer
            memcpy(pke, "Pairwise key expansion", 23);
            if (memcmp(phdsk->smac, phdsk->amac, 6) < 0)
            {
               memcpy(pke+23, phdsk->smac, 6);
               memcpy(pke+29, phdsk->amac, 6);
            }
            else
            {
               memcpy(pke+23, phdsk->amac, 6);
               memcpy(pke+29, phdsk->smac, 6);
            }

            if (memcmp(phdsk->snonce, phdsk->anonce, 32) < 0)
            {
               memcpy(pke+35, phdsk->snonce, 32);
               memcpy(pke+67, phdsk->anonce, 32);
            }
            else
            {
               memcpy(pke+35, phdsk->anonce, 32);
               memcpy(pke+67, phdsk->snonce, 32);
            }

            // calculate the PTK
            for (i=0; i<4; i++)
            {
               pke[99] = i;
               HMAC(EVP_sha1(), pmk, 32, pke, 100, ptk + i*20, NULL);
            }

            // calculate the MIC
            if (phdsk->keyver == 1)
               HMAC(EVP_md5(), ptk, 16, phdsk->eapol, phdsk->eapol_size, mic, NULL);
            else
               HMAC(EVP_sha1(), ptk, 16, phdsk->eapol, phdsk->eapol_size, mic, NULL);

            // check if MIC agrees
            if (memcmp(mic, phdsk->keymic, 16) == 0)
            {
               memcpy(final_key, key, strlen(key));
               *final_key_flag = 1;
               break;
            }
            //closes the while loop through the result set
         }


         if(mysqlResult)
         {
            mysql_free_result(mysqlResult);
            mysqlResult = NULL;
         }
         //closes the try
      }
      catch ( FFError e )
      {
         printf("%s\n",e.Label.c_str());
         mysql_close(MySQLConnection);
         return NULL;
      }
      // check if the final key is found
      if (*final_key_flag)
         break;
      //close the big while
   }
   // Close datbase connection
   mysql_close(MySQLConnection);
/*}
else
{
   fprintf(stderr,"MySQLConnection is null\n");
   //return NULL;
}*/
calc_speed[cpu_core_id] = -1; // this indicates the thread is returned
return NULL;
}


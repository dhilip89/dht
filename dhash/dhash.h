#ifndef _DHASH_H_
#define _DHASH_H_
/*
 *
 * Copyright (C) 2001  Frank Dabek (fdabek@lcs.mit.edu), 
 *                     Frans Kaashoek (kaashoek@lcs.mit.edu),
 *   		       Massachusetts Institute of Technology
 * 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <arpc.h>
#include <async.h>
#include <dhash_prot.h>
#include <chord_prot.h>
#include <dbfe.h>
#include <callback.h>
#include <refcnt.h>
#include <chord.h>
#include <qhash.h>
#include <sys/time.h>
#include <chord.h>
/*
 *
 * dhash.h
 *
 * Include file for the distributed hash service
 */

struct store_cbstate;

typedef callback<void, int, ptr<dbrec>, dhash_stat>::ptr cbvalue;
typedef callback<void, struct store_cbstate *,dhash_stat>::ptr cbstat;
typedef callback<void,dhash_stat>::ptr cbstore;
typedef callback<void,dhash_stat>::ptr cbstat_t;

extern unsigned int MTU;

struct store_chunk {
  store_chunk *next;
  unsigned int start;
  unsigned int end;

  store_chunk (unsigned int s, unsigned int e, store_chunk *n) : next(n), start(s), end(e) {};
  ~store_chunk () {};
};

struct store_state {
  chordID key;
  unsigned int size;
  store_chunk *have;
  char *buf;
  route path;

  ihash_entry <store_state> link;
   
  store_state (chordID k, unsigned int z) : key(k), 
    size(z), have(0), buf(New char[z]) { };

  ~store_state () { 
    delete[] buf; 
    store_chunk *cnext;
    for (store_chunk *c=have; c; c=cnext) {
      cnext = c->next;
      delete c;
    }
  };

  bool addchunk (unsigned int start, unsigned int end, void *base);
  bool iscomplete ();
};



struct dhash_block {
  char *data;
  size_t len;
  int hops;

  ~dhash_block () {  delete [] data; }

  dhash_block (const char *buf, size_t buflen)
    : data (New char[buflen]), len (buflen)
  {
    if (buf)
      memcpy (data, buf, len);
  }
};

struct dhash_block_chunk {
  char *chunk_data;
  size_t chunk_len;
  size_t chunk_offset;
  size_t total_len;
  int hops;
  chordID source;
  chordID key;
  int cookie;

  ~dhash_block_chunk () {delete chunk_data; };
  
  dhash_block_chunk (char *cd, size_t clen, size_t coffset, size_t tlen, chordID source) :
    chunk_data (new char[clen]), chunk_len (clen), 
    chunk_offset (coffset), total_len (tlen), source (source) 
  {
    if (chunk_data)
      memcpy (chunk_data, cd, chunk_len);
  };

      dhash_block_chunk () : chunk_data (NULL), chunk_len (0) {};
      
};

struct pk_partial {
  ptr<dbrec> val;
  int bytes_read;
  int cookie;
  ihash_entry <pk_partial> link;

  pk_partial (ptr<dbrec> v, int c) : val (v), 
		bytes_read (0),
		cookie (c) {};
};

class dhashcli;

class dhash {

  int nreplica;
  int kc_delay;
  int rc_delay;
  int ss_mode;
  int pk_partial_cookie;

  dbfe *db;
  vnode *host_node;
  dhashcli *cli;

  ihash<chordID, store_state, &store_state::key, &store_state::link, hashID> pst;
  ihash<int, pk_partial, &pk_partial::cookie, &pk_partial::link> pk_cache;


  void doRPC (chordID ID, rpc_program prog, int procno,
	      ptr<void> in, void *out, aclnt_cb cb);

  void dispatch (svccb *sbp);
  void sync_cb ();

  void storesvc_cb (svccb *sbp, s_dhash_insertarg *arg, dhash_stat err);
  void fetch_cb (int cookie, cbvalue cb,  ptr<dbrec> ret);
  void fetchiter_svc_cb (svccb *sbp, s_dhash_fetch_arg *farg,
			 int cookie, ptr<dbrec> val, dhash_stat stat);

  void append (ptr<dbrec> key, ptr<dbrec> data,
	       s_dhash_insertarg *arg,
	       cbstore cb);
  void append_after_db_store (cbstore cb, chordID k, int stat);
  void append_after_db_fetch (ptr<dbrec> key, ptr<dbrec> new_data,
			      s_dhash_insertarg *arg, cbstore cb,
			      int cookie, ptr<dbrec> data, dhash_stat err);

  void store (s_dhash_insertarg *arg, cbstore cb);
  void store_cb(store_status type, chordID id, cbstore cb, int stat);
  void store_repl_cb (cbstore cb, dhash_stat err);

  void get_keys_traverse_cb (ptr<vec<chordID> > vKeys,
			     chordID mypred,
			     chordID predid,
			     const chordID &key);

  void init_key_status ();
  void transfer_initial_keys ();
  void transfer_init_getkeys_cb (chordID succ,
				 dhash_getkeys_res *res, 
				 clnt_stat err);
  void transfer_init_gotk_cb (dhash_stat err);

  void update_replica_list ();
  bool isReplica(chordID id);
  void replicate_key (chordID key, cbstat_t cb);
  void replicate_key_cb (unsigned int replicas_done, cbstat_t cb, chordID key,
			 dhash_stat err);


  void install_replica_timer ();
  void check_replicas_cb ();
  void check_replicas ();
  void check_replicas_traverse_cb (chordID to, const chordID &key);
  void fix_replicas_txerd (dhash_stat err);

  void change_status (chordID key, dhash_stat newstatus);

  void transfer_key (chordID to, chordID key, store_status stat, 
		     callback<void, dhash_stat>::ref cb);
  void transfer_fetch_cb (chordID to, chordID key, store_status stat, 
			  callback<void, dhash_stat>::ref cb,
			  int cookie, ptr<dbrec> data, dhash_stat err);
  void transfer_store_cb (callback<void, dhash_stat>::ref cb, 
			  bool err, chordID blockID);

  void get_key (chordID source, chordID key, cbstat_t cb);
  void get_key_got_block (chordID key, cbstat_t cb, ptr<dhash_block> block);
  void get_key_stored_block (cbstat_t cb, int err);

  void store_flush (chordID key, dhash_stat value);
  void store_flush_cb (int err);
  void cache_flush (chordID key, dhash_stat value);
  void cache_flush_cb (int err);

  void transfer_key_cb (chordID key, dhash_stat err);

  char responsible(const chordID& n);

  void printkeys ();
  void printkeys_walk (const chordID &k);
  void printcached_walk (const chordID &k);

  int dbcompare (ref<dbrec> a, ref<dbrec> b);

  ref<dbrec> id2dbrec(chordID id);
  chordID dbrec2id (ptr<dbrec> r);

  chordID pred;
  vec<chordID> replicas;
  timecb_t *check_replica_tcb;

  /* statistics */
  long bytes_stored;
  long keys_stored;
  long keys_replicated;
  long keys_cached;
  long bytes_served;
  long keys_served;
  long rpc_answered;

 public:
  dhash (str dbname, vnode *node, 
	 int nreplica = 0, int ss = 10000, int cs = 1000, int ss_mode = 0);
  void accept(ptr<axprt_stream> x);

  void print_stats ();
  void stop ();
  void fetch (chordID id, int cookie, cbvalue cb);
  dhash_stat key_status(const chordID &n);

  static bool verify (chordID key, dhash_ctype t, char *buf, int len);
  static bool verify_content_hash (chordID key,  char *buf, int len);
  static bool verify_key_hash (chordID key, char *buf, int len);
  static bool verify_dnssec ();
  static ptr<dhash_block> get_block_contents (ptr<dbrec> d, dhash_ctype t);
  static ptr<dhash_block> get_block_contents (ptr<dhash_block> block, dhash_ctype t);
  static ptr<dhash_block> get_block_contents (char *data, unsigned int len, dhash_ctype t);


  static dhash_ctype block_type (ptr<dbrec> d);
  static dhash_ctype block_type (ref<dhash_block> d);
  static dhash_ctype block_type (ptr<dhash_block> d);
  static dhash_ctype block_type (char *value, unsigned int len);
};



typedef callback<void, bool, chordID>::ref cbinsert_t;
typedef cbinsert_t cbstore_t;
typedef callback<void, ptr<dhash_block> >::ref cbretrieve_t;
typedef callback<void, dhash_stat, route, ptr<dhash_block_chunk> >::ref dhashcli_lookup_itercb_t;
typedef callback<void, ptr<dhash_storeres> >::ref dhashcli_storecb_t;
typedef callback<void, dhash_stat, chordID>::ref dhashcli_lookupcb_t;
typedef callback<void, dhash_stat, chordID, route>::ref dhashcli_routecb_t;

class dhashcli {
  ptr<chord> clntnode;
  bool do_cache;

 private:
  void doRPC (chordID ID, rpc_program prog, int procno, ptr<void> in, void *out, aclnt_cb cb);
  void lookup_iter_with_source_cb (ptr<dhash_fetchiter_res> fres, dhashcli_lookup_itercb_t cb, clnt_stat err);
  void lookup_iter_cb (chordID blockID, dhashcli_lookup_itercb_t cb, ref<dhash_fetchiter_res> res, 
		       route path, int nerror,  clnt_stat err);
  void lookup_iter_chalok_cb (ptr<s_dhash_fetch_arg> arg,
			      dhashcli_lookup_itercb_t cb,
			      route path,
			      int nerror,
			      chordID next, bool ok, chordstat s);
  void lookup_findsucc_cb (chordID blockID, dhashcli_lookupcb_t cb, chordID succID, route path, chordstat err);
  void lookup_findsucc_route_cb (chordID blockID, dhashcli_routecb_t cb,
				 chordID succID, route path, chordstat err);
  void store_cb (dhashcli_storecb_t cb, ref<dhash_storeres> res,  clnt_stat err);
  chordID next_hop (chordID k, bool cachedsucc);

 public:
  dhashcli (ptr<chord> node, bool do_cache) : clntnode (node), do_cache (do_cache) {}
  void retrieve (chordID blockID, bool usecachedsucc, cbretrieve_t cb);
  void retrieve (chordID source, chordID blockID, cbretrieve_t cb);
  void insert (chordID blockID, ref<dhash_block> block, 
               bool usecachedsucc, cbinsert_t cb);
  void lookup_iter (chordID sourceID, chordID blockID, uint32 off,
                    uint32 len, bool usecachedsucc, int cookie,
		    dhashcli_lookup_itercb_t cb);
  void lookup_iter (chordID blockID, uint32 off, uint32 len,
                    bool usecachedsucc, dhashcli_lookup_itercb_t cb);
  void storeblock (chordID dest, chordID blockID, ref<dhash_block> block,
		   cbstore_t cb, store_status stat = DHASH_STORE);
  void store (chordID destID, chordID blockID, char *data, size_t len,
              size_t off, size_t totsz, dhash_ctype ctype, store_status store_type,
	      dhashcli_storecb_t cb);
  void lookup (chordID blockID, bool usecachedsucc, dhashcli_lookupcb_t cb);
  void lookup_route (chordID blockID, bool usedcachedsucc, dhashcli_routecb_t cb); 
  //same as above ::lookup but returns the whole route in callback
};



class dhashclient {
private:
  ptr<aclnt> gwclnt;

  // inserts under the specified key
  // (buf need not remain involatile after the call returns)
  void insert (bigint key, const char *buf, size_t buflen, 
	       cbinsert_t cb,  dhash_ctype t, bool usecachedsucc);
  void insertcb (cbinsert_t cb, bigint key, dhash_stat *res, clnt_stat err);
  void retrievecb (cbretrieve_t cb, bigint key,  
		   ref<dhash_retrieve_res> res, clnt_stat err);

public:
  // sockname is the unix path (ex. /tmp/chord-sock) used
  // to communicate to lsd. 
  dhashclient(str sockname);

  void append (chordID to, const char *buf, size_t buflen, cbinsert_t cb);

  // inserts under the contents hash. 
  // (buf need not remain involatile after the call returns)
  void insert (const char *buf, size_t buflen, cbinsert_t cb,
               bool usecachedsucc = false);
  void insert (bigint key, const char *buf, size_t buflen, cbinsert_t cb,
               bool usecachedsucc = false);

  //insert under hash of public key
  void insert (const char *buf, size_t buflen, rabin_priv key, cbinsert_t cb,
               bool usecachedsucc = false);
  void insert (const char *buf, size_t buflen, bigint sig, rabin_pub key,
               cbinsert_t cb, bool usecachedsucc = false);
  void insert (bigint hash, const char *buf, size_t buflen, bigint sig,
               rabin_pub key, cbinsert_t cb, bool usecachedsucc = false);

  // retrieve block and verify
  void retrieve (bigint key, cbretrieve_t cb,
                 bool usecachedsucc = false);

  // synchronouslly call setactive.
  // Returns true on error, and false on success.
  bool sync_setactive (int32 n);
};





class dhashgateway {
  ptr<asrv> clntsrv;
  ptr<chord> clntnode;
  ptr<dhashcli> dhcli;

  void dispatch (svccb *sbp);
  void insert_cb (svccb *sbp, bool err, chordID blockID);
  void retrieve_cb (svccb *sbp, ptr<dhash_block> block);

public:
  dhashgateway (ptr<axprt_stream> x, ptr<chord> clnt, bool do_cache = false);
};


bigint compute_hash (const void *buf, size_t buflen);


static inline str dhasherr2str (dhash_stat status)
{
  return rpc_print (strbuf (), status, 0, NULL, NULL);
}

#endif

#ifndef REPL_STATE_H
#define REPL_STATE_H

#define CHECKBYPASS 0
#define CHECKREPLACE 1
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This file is distributed as part of the Cache Replacement Championship     //
// workshop held in conjunction with ISCA'2010.                               //
//                                                                            //
//                                                                            //
// Everyone is granted permission to copy, modify, and/or re-distribute       //
// this software.                                                             //
//                                                                            //
// Please contact Aamer Jaleel <ajaleel@gmail.com> should you have any        //
// questions                                                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <cassert>
#include "utils.h"
#include "crc_cache_defs.h"
#include <iostream>
#include <vector>

using namespace std;

#define SAMPLER_CACHE_SETS 64
#define SAMPLER_CACHE_WAYS 16
#define CACHE_SETS 4096

// Replacement Policies Supported
typedef enum
{
    CRC_REPL_LRU        = 0,
    CRC_REPL_RANDOM     = 1,
    CRC_REPL_CONTESTANT = 2
} ReplacemntPolicy;

// Replacement State Per Cache Line
typedef struct
{
    UINT32  LRUstackposition;
    bool  blockalive;
    // CONTESTANTS: Add extra state per cache line here
} LINE_REPLACEMENT_STATE;

typedef struct
{
    UINT32  LRUstackposition;
    bool  blockalive;
    // CONTESTANTS: Add extra state per cache line here
    int Yout;
    vector<int> index_of_feature;
    Addr_t tag;
} SAMPLER_REPLACEMENT_STATE;

class PREDICTOR{
public:
  PREDICTOR(){
    m_vfeature1.resize(256, 0);
    m_vfeature2.resize(256, 0);
    m_vfeature3.resize(256, 0);
    m_vfeature4.resize(256, 0);
    m_vfeature5.resize(256, 0);
    m_vfeature6.resize(256, 0);
    tao_bypass= 15;
    tao_replace= 124;
    theta= 66;
  };
  int tao_bypass;  //for predicting block bypass
  int tao_replace; //for predicting block replace
  int theta;
  Addr_t m_history[3];
  vector<int> m_vfeature1;
  vector<int> m_vfeature2;
  vector<int> m_vfeature3;
  vector<int> m_vfeature4;
  vector<int> m_vfeature5;
  vector<int> m_vfeature6;

  int predict(Addr_t currPC, Addr_t currLine, int whichtao);
  int get_weight(int whichfeature, int index);
  void update_weight(int whichfeature, int index, int addorsub);
  vector<int> getIndex(Addr_t currPC, Addr_t currLine);
  void update_history(Addr_t currPC);
};
//struct sampler; // Jimenez's structures

// The implementation for the cache replacement policy
class CACHE_REPLACEMENT_STATE
{
public:
    LINE_REPLACEMENT_STATE   **repl;
    SAMPLER_REPLACEMENT_STATE   **sampler;
  private:

    UINT32 numsets;
    UINT32 assoc;
    UINT32 replPolicy;

    COUNTER mytimer;  // tracks # of references to the cache

    // CONTESTANTS:  Add extra state for cache here

  public:
    ostream & PrintStats(ostream &out);
    PREDICTOR *m_predict;
    // The constructor CAN NOT be changed
    CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol );
    CACHE_REPLACEMENT_STATE(){};

    INT32 GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType );

    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID );

    void   SetReplacementPolicy( UINT32 _pol ) { replPolicy = _pol; }
    void   IncrementTimer() { mytimer++; }

    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine,
                                   UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit );

    ~CACHE_REPLACEMENT_STATE(void);

  private:

    void   InitReplacementState();
    INT32  Get_Random_Victim( UINT32 setIndex );

    INT32  Get_LRU_Victim( UINT32 setIndex );
    INT32  Get_My_Victim( UINT32 setIndex );
    void   UpdateLRU( UINT32 setIndex, INT32 updateWayID );

    void   UpdateMyPolicy( UINT32 setIndex, INT32 updateWayID,
      vector<int> _index, Addr_t _tag, int _Yout);

    void   UpdateLRU( UINT32 setIndex, INT32 updateWayID, int blockalive );
};

#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <map>
#include <iostream>
#include <vector>

using namespace std;

#include "replacement_state.h"

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

/*
** This file implements the cache replacement state. Users can enhance the code
** below to develop their cache replacement ideas.
**
*/


////////////////////////////////////////////////////////////////////////////////
// The replacement state constructor:                                         //
// Inputs: number of sets, associativity, and replacement policy to use       //
// Outputs: None                                                              //
//                                                                            //
// DO NOT CHANGE THE CONSTRUCTOR PROTOTYPE                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
CACHE_REPLACEMENT_STATE::CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol )
{
    numsets    = _sets;
    assoc      = _assoc;
    replPolicy = _pol;

    mytimer    = 0;
    m_predict = new PREDICTOR;

    InitReplacementState();
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// The function prints the statistics for the cache                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
ostream & CACHE_REPLACEMENT_STATE::PrintStats(ostream &out)
{

    out<<"=========================================================="<<endl;
    out<<"=========== Replacement Policy Statistics ================"<<endl;
    out<<"=========================================================="<<endl;

    // CONTESTANTS:  Insert your statistics printing here

    return out;

}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function initializes the replacement policy hardware by creating      //
// storage for the replacement state on a per-line/per-cache basis.           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void CACHE_REPLACEMENT_STATE::InitReplacementState()
{
    // Create the state for sets, then create the state for the ways
    repl  = new LINE_REPLACEMENT_STATE* [ numsets ];
    sampler = new SAMPLER_REPLACEMENT_STATE* [ SAMPLER_CACHE_SETS ];
    // ensure that we were able to create replacement state

    assert(repl);

    // Create the state for the sets
    for(UINT32 setIndex=0; setIndex<numsets; setIndex++)
    {
        repl[ setIndex ] = new LINE_REPLACEMENT_STATE[ assoc ];

        for(UINT32 way=0; way<assoc; way++)
        {
            // initialize stack position (for true LRU)
            repl[ setIndex ][ way ].LRUstackposition = way;
            repl[ setIndex ][ way ].blockalive = false;
        }
    }

    for(UINT32 setIndex=0; setIndex<SAMPLER_CACHE_SETS; setIndex++){
      sampler[ setIndex ] = new SAMPLER_REPLACEMENT_STATE[ SAMPLER_CACHE_WAYS ];

      for(UINT32 way=0; way<SAMPLER_CACHE_WAYS; way++)
      {
          //init sampler, sampler has 64 sets
          sampler[ setIndex ][ way ].LRUstackposition = way;
          sampler[ setIndex ][ way ].blockalive = false;
          sampler[ setIndex ][ way ].index_of_feature.resize(6,0); //6 feature tables
          sampler[ setIndex ][ way ].tag= 0;
          sampler[ setIndex ][ way ].Yout= 0;
      }
    }

    if (replPolicy != CRC_REPL_CONTESTANT) return;

    // Contestants:  ADD INITIALIZATION FOR YOUR HARDWARE HERE
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache on every cache miss. The input        //
// argument is the set index. The return value is the physical way            //
// index for the line being replaced.                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc,
  Addr_t PC, Addr_t paddr, UINT32 accessType ) {
    // If no invalid lines, then replace based on replacement policy
    //printf("enter get victim\n");
    if( replPolicy == CRC_REPL_LRU )
    {
        return Get_LRU_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        return Get_Random_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_CONTESTANT )
    {
      if(accessType == ACCESS_PREFETCH || accessType == ACCESS_WRITEBACK)
        return -1;
        // Contestants:  ADD YOUR VICTIM SELECTION FUNCTION HERE
        Addr_t tag= paddr >> 18;
        int _return;
        _return = m_predict->predict(PC, tag, CHECKBYPASS);
        if(_return != -1){
          _return = Get_My_Victim (setIndex);
          if(_return == -1){
            _return = Get_LRU_Victim(setIndex);
          }
          return _return;
        }
        else
          return _return;
    }

    // We should never here here

    assert(0);
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache after every cache hit/miss            //
// The arguments are: the set index, the physical way of the cache,           //
// the pointer to the physical line (should contestants need access           //
// to information of the line filled or hit upon), the thread id              //
// of the request, the PC of the request, the accesstype, and finall          //
// whether the line was a cachehit or not (cacheHit=true implies hit)         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateReplacementState(
    UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine,
    UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit )
{
	//fprintf (stderr, "ain't I a stinker? %lld\n", get_cycle_count ());
	//fflush (stderr);
    // What replacement policy?
    if( replPolicy == CRC_REPL_LRU )
    {
        UpdateLRU( setIndex, updateWayID );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        // Random replacement requires no replacement state update
    }
    else if( replPolicy == CRC_REPL_CONTESTANT )
    {
        UINT32 InSampler= setIndex%SAMPLER_CACHE_SETS;
        if(InSampler == 0){
          UINT32 setInSampler= setIndex/SAMPLER_CACHE_SETS;
          Addr_t curr_tag= currLine->tag;
          bool samplerHit= false;
          int hitway=0;
          for(UINT32 way=0; way<SAMPLER_CACHE_WAYS; way++)
          {
              //check current block is in sampler set
              if(sampler[ setInSampler ][ way ].tag == curr_tag){
                //printf("%llu\n", sampler[ setInSampler ][ way ].tag );
                samplerHit = true;
                hitway = way;
              }
          }

          if(samplerHit){
              vector<int> index;
              index = sampler[ setInSampler ][ hitway ].index_of_feature;
              int final_weight=0;

              for(int i= 1; i<=6; i++){
                final_weight+= m_predict->get_weight(i, index[i-1]);
              }

              int negative= -1 * m_predict->theta;
              if(final_weight > negative){
                for(int i=1; i<=6; i++){
                  //last value -1 is because we need to lower the total weight to keep block alive
                  m_predict->update_weight(i, index[i-1], -5);
                }
              }
              UpdateMyPolicy( setInSampler, hitway, index, curr_tag, final_weight);

              int blockalive = (final_weight <= m_predict->tao_replace)? true: false;
              UpdateLRU(setIndex, updateWayID, blockalive);

          }
          else{
              //can't find block in sampler
              //update old index predict table value
              //change sampler block
              //update new index predict table value

              SAMPLER_REPLACEMENT_STATE predict_block= sampler[ setInSampler ][ updateWayID ];

              int final_weight=0;
              for(int i= 1; i<=6; i++){
                final_weight+= m_predict->get_weight(i, predict_block.index_of_feature[i-1]);
              }

              //----------------------------check if the prediction incorrect-----------------------
              //prediction direction
              bool direction_same = ((final_weight <= m_predict->tao_replace) && (cacheHit))? 1:-1;
              if(predict_block.Yout <= m_predict->theta || direction_same){
                for(int i= 1; i<= 6; i++){
                  //last value 1 is because we need to increase the total weight to predict block dead
                  m_predict->update_weight(i, predict_block.index_of_feature[i-1], 1);
                }
              }
              //change sampler block
              vector<int> index= m_predict->getIndex(PC, currLine->tag);

              UpdateMyPolicy(setInSampler, updateWayID, index, currLine->tag, final_weight);
              int blockalive = (final_weight <= m_predict->tao_replace)? true: false;
              UpdateLRU(setIndex, updateWayID, blockalive);
          }

        }
        else{
          //in real cache
          if(cacheHit){
              int final_weight=0;
              vector<int> index= m_predict->getIndex(PC, currLine->tag);

              for(int i= 1; i<=6; i++){
                final_weight+= m_predict->get_weight(i, index[i-1]);
              }

              if(final_weight <= m_predict->tao_replace)
                repl[ setIndex ][ updateWayID ].blockalive= 1;
              else
                repl[ setIndex ][ updateWayID ].blockalive= 0;

              UpdateLRU(setIndex, updateWayID);
          }
          else{

              int final_weight=0;
              vector<int> index= m_predict->getIndex(PC, currLine->tag);

              for(int i= 1; i<=6; i++){
                final_weight+= m_predict->get_weight(i, index[i-1]);
              }

              //change sampler block
              if(final_weight <= m_predict->tao_replace)
                repl[ setIndex ][ updateWayID ].blockalive= 1;
              else
                repl[ setIndex ][ updateWayID ].blockalive= 0;

              UpdateLRU(setIndex, updateWayID);
          }

        }
    }
    m_predict->update_history(PC);
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//////// HELPER FUNCTIONS FOR REPLACEMENT UPDATE AND VICTIM SELECTION //////////
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds the LRU victim in the cache set by returning the       //
// cache block at the bottom of the LRU stack. Top of LRU stack is '0'        //
// while bottom of LRU stack is 'assoc-1'                                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_LRU_Victim( UINT32 setIndex )
{
	// Get pointer to replacement state of current set

	LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
	INT32   lruWay   = 0;

	// Search for victim whose stack position is assoc-1

	for(UINT32 way=0; way<assoc; way++) {
		if (replSet[way].LRUstackposition == (assoc-1)) {
			lruWay = way;
			break;
		}
	}

	// return lru way

	return lruWay;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds a random victim in the cache set                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_Random_Victim( UINT32 setIndex )
{
    INT32 way = (rand() % assoc);

    return way;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function implements the LRU update routine for the traditional        //
// LRU replacement policy. The arguments to the function are the physical     //
// way and set index.                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void CACHE_REPLACEMENT_STATE::UpdateLRU( UINT32 setIndex, INT32 updateWayID )
{
	// Determine current LRU stack position
	UINT32 currLRUstackposition = repl[ setIndex ][ updateWayID ].LRUstackposition;

	// Update the stack position of all lines before the current line
	// Update implies incremeting their stack positions by one

	for(UINT32 way=0; way<assoc; way++) {
		if( repl[setIndex][way].LRUstackposition < currLRUstackposition ) {
			repl[setIndex][way].LRUstackposition++;
		}
	}

	// Set the LRU stack position of new line to be zero
	repl[ setIndex ][ updateWayID ].LRUstackposition = 0;
}

void CACHE_REPLACEMENT_STATE::UpdateLRU( UINT32 setIndex, INT32 updateWayID, int blockalive ){
  // Determine current LRU stack position
	UINT32 currLRUstackposition = repl[ setIndex ][ updateWayID ].LRUstackposition;

	// Update the stack position of all lines before the current line
	// Update implies incremeting their stack positions by one

	for(UINT32 way=0; way<assoc; way++) {
		if( repl[setIndex][way].LRUstackposition < currLRUstackposition ) {
			repl[setIndex][way].LRUstackposition++;
		}
	}

	// Set the LRU stack position of new line to be zero
	repl[ setIndex ][ updateWayID ].LRUstackposition = 0;
  repl[ setIndex ][ updateWayID ].blockalive= blockalive;
}


INT32 CACHE_REPLACEMENT_STATE::Get_My_Victim( UINT32 setIndex ) {

  //1. check if there's empty block to put
  //2. check if have dead block
  //3. check LRU
  LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
	INT32 _returnWay = -1;

	for(UINT32 way=0; way<assoc; way++) {
    if (replSet[way].blockalive == false){
      _returnWay = way;
      break;
    }
	}

	return _returnWay;
}

void CACHE_REPLACEMENT_STATE::UpdateMyPolicy( UINT32 setIndex, INT32 updateWayID,
    vector<int> _index, Addr_t _tag, int _Yout){
  // Determine current LRU stack position
  UINT32 currLRUstackposition = sampler[ setIndex ][ updateWayID ].LRUstackposition;

  for(UINT32 way=0; way<SAMPLER_CACHE_WAYS; way++) {
    if( sampler[setIndex][way].LRUstackposition < currLRUstackposition ) {
      sampler[setIndex][way].LRUstackposition++;
    }
  }

  sampler[ setIndex ][ updateWayID ].LRUstackposition = 0;
  sampler[ setIndex ][ updateWayID ].index_of_feature= _index;
  sampler[ setIndex ][ updateWayID ].tag= _tag;
  sampler[ setIndex ][ updateWayID ].Yout= _Yout;
}

CACHE_REPLACEMENT_STATE::~CACHE_REPLACEMENT_STATE (void) {
}


int PREDICTOR::predict(Addr_t currPC, Addr_t currLine, int whichtao){
  int _result=-1;
  vector<int> index= getIndex(currPC, currLine);
  int final_weight=0;
  for(int i=1; i<=6; i++){
    final_weight+= get_weight(i, index[i-1]);
  }
  _result=  (final_weight<= tao_bypass)? 1: -1;
  return _result;
}

int PREDICTOR::get_weight(int whichfeature, int index){
    int returnweight= 0;
    switch (whichfeature) {
      case 1:
        returnweight= m_vfeature1[index];
        break;
      case 2:
        returnweight= m_vfeature2[index];
      break;
      case 3:
        returnweight= m_vfeature3[index];
        break;
      case 4:
        returnweight= m_vfeature4[index];
        break;
      case 5:
        returnweight= m_vfeature5[index];
        break;
      case 6:
        returnweight= m_vfeature6[index];
        break;
      default:
        printf("get_weight, the featureindex is not valid\n" );
        break;
    }
    return returnweight;
}

void PREDICTOR::update_weight(int whichfeature, int index, int addorsub){

  switch (whichfeature) {
    case 1:
      if(abs(m_vfeature1[index]) < 32)
        m_vfeature1[index]+=addorsub;
      break;
    case 2:
      if(abs(m_vfeature2[index]) < 32)
        m_vfeature2[index]+=addorsub;
      break;
    case 3:
      if(abs(m_vfeature3[index]) < 32)
        m_vfeature3[index]+=addorsub;
      break;
    case 4:
      if(abs(m_vfeature4[index]) < 32)
        m_vfeature4[index]+=addorsub;
      break;
    case 5:
      if(abs(m_vfeature5[index]) < 32)
        m_vfeature5[index]+=addorsub;
      break;
    case 6:
      if(abs(m_vfeature6[index]) < 32)
        m_vfeature6[index]+=addorsub;
      break;
    default:
      printf("update_weight, the featureindex is not valid\n" );
      break;
  }

}

vector<int> PREDICTOR::getIndex(Addr_t currPC, Addr_t currLine){

  Addr_t feature1 = ((currPC >> 2) ^ currPC)%256;
  Addr_t feature2 = ((m_history[0] >>1) ^ currPC)%256;
  Addr_t feature3 = ((m_history[1] >>2) ^ currPC)%256;
  Addr_t feature4 = ((m_history[2] >>3) ^ currPC)%256;
  Addr_t feature5 = ((currLine >> 2) ^ currPC)%256;
  Addr_t feature6 = ((currLine >> 5) ^ currPC)%256;
  vector<int> _return;
  _return.push_back(feature1);
  _return.push_back(feature2);
  _return.push_back(feature3);
  _return.push_back(feature4);
  _return.push_back(feature5);
  _return.push_back(feature6);

  return _return;
}

void PREDICTOR::update_history(Addr_t currPC){
  m_history[2]= m_history[1];
  m_history[1]= m_history[0];
  m_history[0]= currPC;
}

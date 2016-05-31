#include "rate-limiter.h"
#include <ts/ts.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "debug_macros.h"

namespace ATS_RL {

  int RateLimiter::AddCounter(float max_rate, uint64_t milliseconds) {
    LimiterEntry * entry = new LimiterEntry(max_rate, milliseconds);
    counters_.push_back(entry);
    return counters_.size()-1;
  }

  LimiterState * RateLimiter::Register() {
    timeval now;
    gettimeofday(&now,NULL);
	LimiterState * state =  new LimiterState(GetCounterArray(), GetTimevalArray(now));
    return state;
  }

  uint64_t RateLimiter::GetMaxUnits(uint64_t amount,LimiterState * state) {
    timeval timev;
    gettimeofday(&timev,NULL);
    time_t t = time(NULL);
	struct tm * p = localtime(&t); 
    int counter_index = p->tm_hour;
    LimiterEntry * limiter_entry = counters_[counter_index];

    if (!state) {
      return limiter_entry->max_rate();
    }

	TSMutexLock(update_mutex_);
    timeval elapsed;
    timeval stime = state->time(counter_index);
    timersub(&timev, &stime, &elapsed);

    float elapsed_ms = (elapsed.tv_sec * 1000.0f) + ( elapsed.tv_usec/1000.0f );
    float rate_timeslice = 1.0f - (limiter_entry->milliseconds() - elapsed_ms) / limiter_entry->milliseconds();
    if (rate_timeslice<0) rate_timeslice=0;

    float replenishment  = rate_timeslice * limiter_entry->max_rate();
    float newallowance   = state->allowance(counter_index) + replenishment;

    newallowance = newallowance > limiter_entry->max_rate() ? limiter_entry->max_rate() : newallowance;

    int rv = amount;

    if (amount > newallowance) {
      amount = rv = newallowance;
    }
	
    newallowance -= amount;
    
    if (newallowance >= 0.0f  ) {
      state->set_allowance(counter_index, newallowance);
      state->set_time(counter_index, timev);
    }
	TSMutexUnlock(update_mutex_);
    return rv;
  }

  float * RateLimiter::GetCounterArray() {
    size_t size = counters_.size() * sizeof(float);
    float * a = (float *)malloc(size);
    memset(a,0,size);
    for(size_t i = 0; i < counters_.size(); i++) { 
      a[i] = counters_[i]->max_rate();
    }
    return a;
  }

  timeval * RateLimiter::GetTimevalArray(timeval time) {
    size_t size = counters_.size() * sizeof(timeval);
    timeval * a = (timeval *)malloc(size);
    memset(a,0,size);
    for(size_t i = 0; i < counters_.size(); i++) { 
      a[i] = time;
    }
    return a;
  }

  LimiterEntry * RateLimiter::GetCounter(int index) {
    return counters_[index];
  }
}




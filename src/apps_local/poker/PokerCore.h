#pragma once

// Freestanding, deterministic No-Limit Texas Hold'em rules for CrossInk Play.
// All stacks are virtual chips. No persistence or network dependencies.
// Card bytes use the fork's shared cards:: encoding (Ace is rank index 0).

#include <cstddef>
#include <cstdint>
#include <cstring>
#include "../cards/Cards.h"

namespace poker {
constexpr int kMaxPlayers = 6;
constexpr uint8_t kNone = cards::kNoCard;
enum class Action : uint8_t { Fold = 0, CheckCall = 1, RaiseTo = 2 };
enum class Street : uint8_t { Preflop = 0, Flop = 1, Turn = 2, River = 3 };

struct Seat {
  uint32_t stack = 0;
  uint32_t total = 0;
  uint32_t bet = 0;
  uint32_t facedAt = 0;
  uint8_t hole[2] = {kNone, kNone};
  bool folded = true;
  bool allIn = false;
  bool acted = false;
};

struct Game {
  Seat seat[kMaxPlayers]{};
  uint8_t deck[52]{};
  uint8_t board[5] = {kNone, kNone, kNone, kNone, kNone};
  uint8_t deckAt = 0;
  uint8_t players = 2;
  uint8_t humans = 1;
  uint8_t boardCount = 0;
  Street street = Street::Preflop;
  int8_t button = -1;
  int8_t turn = -1;
  int8_t lastWinner = -1;
  uint32_t lastWon = 0;
  uint32_t smallBlind = 10;
  uint32_t bigBlind = 20;
  uint32_t currentBet = 0;
  uint32_t lastFullRaise = 20;
  uint32_t hands = 0;
  uint32_t revision = 0;
  uint32_t rng = 1;
  bool handOver = true;
  bool tableOver = false;
};

inline uint32_t random(Game& g) {
  uint32_t x = g.rng ? g.rng : 0x6d2b79f5u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return g.rng = x;
}

inline uint8_t draw(Game& g) {
  return g.deckAt < 52 ? g.deck[g.deckAt++] : kNone;
}

inline int nextLive(const Game& g, int after) {
  for (int n = 1; n <= g.players; ++n) {
    const int i = (after + n + g.players) % g.players;
    if (g.seat[i].stack) return i;
  }
  return -1;
}
inline int stillIn(const Game& g) {
  int n = 0;
  for (int i = 0; i < g.players; ++i)
    if (!g.seat[i].folded) ++n;
  return n;
}
inline int activeStacks(const Game& g) {
  int n = 0;
  for (int i = 0; i < g.players; ++i)
    if (g.seat[i].stack) ++n;
  return n;
}
inline int canStillBet(const Game& g) {
  int n = 0;
  for (int i = 0; i < g.players; ++i)
    if (!g.seat[i].folded && !g.seat[i].allIn) ++n;
  return n;
}
inline uint32_t pot(const Game& g) {
  uint32_t n = 0;
  for (int i = 0; i < g.players; ++i) n += g.seat[i].total;
  return n;
}
inline uint32_t chips(const Game& g) {
  uint32_t n = 0;
  for (int i = 0; i < g.players; ++i) n += g.seat[i].total + g.seat[i].stack;
  return n;
}
inline int value(uint8_t c) {
  const int rank = cards::rankOf(c);
  return rank == cards::kAce ? 14 : rank + 1; // Two=2 ... King=13
}

// Compact comparable five-card rank. Category is the high nibble:
// high-card=0, pair=1 ... straight-flush=8. Five kickers follow, high-first.
inline uint32_t evaluate5(const uint8_t hand[5]) {
  int counts[15] = {};
  int suits[4] = {};
  for (int i = 0; i < 5; ++i) {
    if (hand[i] == kNone) return 0;
    ++counts[value(hand[i])];
    ++suits[static_cast<int>(cards::suitOf(hand[i]))];
  }
  const bool flush = (suits[0] == 5 || suits[1] == 5 || suits[2] == 5 || suits[3] == 5);
  int straight = 0;
  for (int hi = 14; hi >= 5; --hi) {
    bool found = true;
    for (int k = 0; k < 5; ++k) {
      const int r = hi - k == 1 ? 14 : hi - k;
      if (!counts[r]) { found = false; break; }
    }
    if (found) { straight = hi; break; }
  }
  int four = 0, three = 0, firstPair = 0, secondPair = 0;
  for (int r = 14; r >= 2; --r) {
    if (counts[r] == 4) four = r;
    else if (counts[r] == 3) three = r;
    else if (counts[r] == 2) {
      if (!firstPair) firstPair = r;
      else secondPair = r;
    }
  }
  auto pack = [](int category, int a=0, int b=0, int c=0, int d=0, int e=0) {
    return (static_cast<uint32_t>(category) << 20) | (static_cast<uint32_t>(a) << 16) |
           (static_cast<uint32_t>(b) << 12) | (static_cast<uint32_t>(c) << 8) |
           (static_cast<uint32_t>(d) << 4) | static_cast<uint32_t>(e);
  };
  if (flush && straight) return pack(8, straight);
  if (four) {
    for (int r = 14; r >= 2; --r) if (counts[r] == 1) return pack(7, four, r);
  }
  if (three && firstPair) return pack(6, three, firstPair);
  if (flush) {
    int kicker[5]={}, k=0;
    for (int r=14;r>=2;--r) if (counts[r]) kicker[k++]=r;
    return pack(5,kicker[0],kicker[1],kicker[2],kicker[3],kicker[4]);
  }
  if (straight) return pack(4,straight);
  if (three) {
    int kicker[2]={},k=0;
    for(int r=14;r>=2;--r) if (counts[r]==1) kicker[k++]=r;
    return pack(3,three,kicker[0],kicker[1]);
  }
  if (firstPair && secondPair) {
    for(int r=14;r>=2;--r) if(counts[r]==1) return pack(2,firstPair,secondPair,r);
  }
  if (firstPair) {
    int kicker[3]={},k=0;
    for(int r=14;r>=2;--r) if(counts[r]==1) kicker[k++]=r;
    return pack(1,firstPair,kicker[0],kicker[1],kicker[2]);
  }
  int kicker[5]={},k=0;
  for(int r=14;r>=2;--r) if(counts[r]) kicker[k++]=r;
  return pack(0,kicker[0],kicker[1],kicker[2],kicker[3],kicker[4]);
}

inline uint32_t evaluate(const uint8_t* cardsIn, int count) {
  if (count < 5 || count > 7) return 0;
  uint32_t best=0;
  uint8_t hand[5];
  for(int a=0;a<count-4;++a)
    for(int b=a+1;b<count-3;++b)
      for(int c=b+1;c<count-2;++c)
        for(int d=c+1;d<count-1;++d)
          for(int e=d+1;e<count;++e) {
            hand[0]=cardsIn[a]; hand[1]=cardsIn[b]; hand[2]=cardsIn[c];
            hand[3]=cardsIn[d]; hand[4]=cardsIn[e];
            const uint32_t score=evaluate5(hand);
            if(score>best) best=score;
          }
  return best;
}
inline uint32_t rankSeat(const Game& g,int i) {
  uint8_t c[7]={g.seat[i].hole[0],g.seat[i].hole[1],kNone,kNone,kNone,kNone,kNone};
  for(int j=0;j<5;++j)c[2+j]=g.board[j];
  return evaluate(c,7);
}
inline bool needsAction(const Game& g,int i) {
  const Seat& s=g.seat[i];
  return !s.folded && !s.allIn && (!s.acted || s.bet < g.currentBet);
}
inline int nextAction(const Game& g,int after) {
  for(int n=1;n<=g.players;++n) {
    const int i=(after+n+g.players)%g.players;
    if(needsAction(g,i))return i;
  }
  return -1;
}
inline uint32_t toCall(const Game& g,int i) {
  return g.currentBet > g.seat[i].bet ? g.currentBet-g.seat[i].bet : 0;
}
inline bool mayRaise(const Game& g,int i) {
  const Seat& s=g.seat[i];
  return !s.folded && !s.allIn && s.bet+s.stack > g.currentBet &&
         (!s.acted || g.currentBet-s.facedAt >= g.lastFullRaise);
}
inline uint32_t minRaiseTo(const Game& g) {
  return g.currentBet ? g.currentBet+g.lastFullRaise : g.bigBlind;
}
inline uint32_t maxRaiseTo(const Game& g,int i) { return g.seat[i].bet+g.seat[i].stack; }

inline void awardUncontested(Game& g) {
  const int winner=[&](){for(int i=0;i<g.players;++i)if(!g.seat[i].folded)return i;return -1;}();
  g.lastWinner=winner;
  g.lastWon=pot(g);
  if(winner>=0) g.seat[winner].stack+=g.lastWon;
  for(int i=0;i<g.players;++i) {g.seat[i].total=0;g.seat[i].bet=0;}
  g.turn=-1;g.handOver=true;g.tableOver=activeStacks(g)<=1;
  ++g.revision;
}

// Side-pots use contribution tiers, including dead money from folded seats.
// Odd chips go clockwise from the button. A one-eligible-player tier refunds
// that seat's uncalled excess (it has no opponent competing for that tier).
inline void showdown(Game& g) {
  const int contenders=stillIn(g);
  if(contenders<=1) {awardUncontested(g);return;}
  uint32_t ranks[kMaxPlayers]={};
  for(int i=0;i<g.players;++i)if(!g.seat[i].folded)ranks[i]=rankSeat(g,i);
  uint32_t won[kMaxPlayers]={};
  uint32_t previous=0;
  for(int tier=0;tier<g.players;++tier) {
    uint32_t level=0xffffffffu;
    for(int i=0;i<g.players;++i)
      if(g.seat[i].total>previous && g.seat[i].total<level)level=g.seat[i].total;
    if(level==0xffffffffu)break;
    int contributors=0;
    for(int i=0;i<g.players;++i)if(g.seat[i].total>=level)++contributors;
    const uint32_t share=(level-previous)*static_cast<uint32_t>(contributors);
    uint32_t best=0;
    for(int i=0;i<g.players;++i)
      if(!g.seat[i].folded && g.seat[i].total>=level && ranks[i]>best)best=ranks[i];
    int ties=0;
    for(int i=0;i<g.players;++i)
      if(!g.seat[i].folded && g.seat[i].total>=level && ranks[i]==best)++ties;
    if(ties) {
      const uint32_t each=share/static_cast<uint32_t>(ties);
      uint32_t remainder=share%static_cast<uint32_t>(ties);
      for(int i=0;i<g.players;++i)
        if(!g.seat[i].folded && g.seat[i].total>=level && ranks[i]==best)won[i]+=each;
      for(int n=1;n<=g.players && remainder;++n) {
        const int i=(g.button+n)%g.players;
        if(!g.seat[i].folded && g.seat[i].total>=level && ranks[i]==best) {
          ++won[i]; --remainder;
        }
      }
    }
    previous=level;
  }
  g.lastWinner=-1;g.lastWon=0;
  for(int i=0;i<g.players;++i) {
    g.seat[i].stack+=won[i];
    if(won[i]>g.lastWon){g.lastWon=won[i];g.lastWinner=i;}
    g.seat[i].total=0;g.seat[i].bet=0;
  }
  g.turn=-1;g.handOver=true;g.tableOver=activeStacks(g)<=1;
  ++g.revision;
}

inline void finishStreet(Game& g) {
  if(stillIn(g)<=1){awardUncontested(g);return;}
  // If one or zero seats can still bet, all remaining community cards
  // are dealt immediately: nobody could call a fresh wager.
  if(g.street==Street::River || canStillBet(g)<2) {
    while(g.boardCount<5) g.board[g.boardCount++]=draw(g);
    g.street=Street::River;
    showdown(g);
    return;
  }
  g.street=static_cast<Street>(static_cast<int>(g.street)+1);
  const int target=g.street==Street::Flop?3:g.street==Street::Turn?4:5;
  while(g.boardCount<target)g.board[g.boardCount++]=draw(g);
  g.currentBet=0;g.lastFullRaise=g.bigBlind;
  for(int i=0;i<g.players;++i){g.seat[i].bet=0;g.seat[i].acted=false;g.seat[i].facedAt=0;}
  g.turn=nextAction(g,g.button);
  if(g.turn<0)finishStreet(g);
}

inline void postBlind(Game& g,int i,uint32_t blind) {
  Seat& s=g.seat[i];
  const uint32_t paid=s.stack<blind?s.stack:blind;
  s.stack-=paid;s.bet+=paid;s.total+=paid;
  s.allIn=s.stack==0;
}

inline void reset(Game& g,int players,int humans,uint32_t startingStack,uint32_t bigBlind,uint32_t seed) {
  g=Game{};
  g.players=static_cast<uint8_t>(players<2?2:players>kMaxPlayers?kMaxPlayers:players);
  g.humans=static_cast<uint8_t>(humans<1?1:humans>2?2:humans);
  g.bigBlind=bigBlind>0?bigBlind:20;
  g.smallBlind=g.bigBlind/2?g.bigBlind/2:1;
  g.lastFullRaise=g.bigBlind;
  g.button=static_cast<int8_t>(g.players-1);
  g.rng=seed?seed:1;
  for(int i=0;i<g.players;++i)g.seat[i].stack=startingStack>=g.bigBlind*2?startingStack:g.bigBlind*2;
}
inline void startHand(Game& g) {
  if(activeStacks(g)<2){g.tableOver=true;g.handOver=true;g.turn=-1;return;}
  g.button=static_cast<int8_t>(nextLive(g,g.button));
  ++g.hands;
  g.boardCount=0;g.street=Street::Preflop;
  for(auto& c:g.board)c=kNone;
  for(int i=0;i<g.players;++i) {
    Seat& s=g.seat[i];s.total=s.bet=s.facedAt=0;
    s.acted=false;s.allIn=false;s.folded=s.stack==0;
    s.hole[0]=s.hole[1]=kNone;
  }
  int k=0;
  for(int suit=0;suit<cards::kSuits;++suit)
    for(int rank=0;rank<cards::kRanks;++rank)g.deck[k++]=cards::makeCard(static_cast<cards::Suit>(suit),rank);
  for(int i=51;i>0;--i) {
    const int j=static_cast<int>(random(g)%static_cast<uint32_t>(i+1));
    const uint8_t temp=g.deck[i];g.deck[i]=g.deck[j];g.deck[j]=temp;
  }
  g.deckAt=0;
  // Deal one hole card to each live seat twice.
  for(int round=0;round<2;++round) {
    int p=g.button;
    for(int n=0;n<g.players;++n) {
      p=(p+1)%g.players;
      if(!g.seat[p].folded)g.seat[p].hole[round]=draw(g);
    }
  }
  const int active=activeStacks(g);
  const int sb=active==2?g.button:nextLive(g,g.button);
  const int bb=nextLive(g,sb);
  postBlind(g,sb,g.smallBlind);
  postBlind(g,bb,g.bigBlind);
  g.currentBet=g.seat[bb].bet>g.seat[sb].bet?g.seat[bb].bet:g.seat[sb].bet;
  g.lastFullRaise=g.bigBlind;
  g.lastWinner=-1;g.lastWon=0;g.handOver=false;g.tableOver=false;
  g.turn=static_cast<int8_t>(nextAction(g,bb));
  if(g.turn<0)finishStreet(g);
  ++g.revision;
}

inline bool act(Game& g,int i,Action action,uint32_t raiseTo=0) {
  if(g.handOver || i!=g.turn || i<0 || i>=g.players)return false;
  Seat& s=g.seat[i];
  const uint32_t call=toCall(g,i);
  if(action==Action::Fold) {
    s.folded=true;s.acted=true;
  } else if(action==Action::CheckCall) {
    const uint32_t paid=s.stack<call?s.stack:call;
    s.stack-=paid;s.bet+=paid;s.total+=paid;
    s.allIn=s.stack==0;s.acted=true;s.facedAt=g.currentBet;
  } else if(action==Action::RaiseTo) {
    if(!mayRaise(g,i) || raiseTo<=g.currentBet || raiseTo>maxRaiseTo(g,i))return false;
    if(raiseTo<minRaiseTo(g) && raiseTo!=maxRaiseTo(g,i))return false;
    const uint32_t previous=g.currentBet;
    const uint32_t paid=raiseTo-s.bet;
    s.stack-=paid;s.bet=raiseTo;s.total+=paid;
    s.allIn=s.stack==0;s.acted=true;s.facedAt=raiseTo;
    g.currentBet=raiseTo;
    const uint32_t increase=raiseTo-previous;
    if(increase>=g.lastFullRaise) {
      g.lastFullRaise=increase;
      for(int j=0;j<g.players;++j)if(j!=i && !g.seat[j].folded && !g.seat[j].allIn)g.seat[j].acted=false;
    }
  } else return false;
  if(stillIn(g)<=1)awardUncontested(g);
  else {
    const int next=nextAction(g,i);
    if(next<0)finishStreet(g);
    else g.turn=static_cast<int8_t>(next);
  }
  ++g.revision;
  return true;
}

// A deliberately lightweight local opponent, not a purported GTO engine.
// Profiles vary by seat: conservative, balanced and loose/aggressive.
struct Decision { Action action=Action::CheckCall; uint32_t raiseTo=0; };
inline Decision botMove(Game& g,int i) {
  Decision d;
  if(g.turn!=i || g.handOver)return d;
  const Seat& s=g.seat[i];
  const int a=value(s.hole[0]),b=value(s.hole[1]);
  int strength=0;
  if(g.boardCount==0) {
    strength=(a>b?a:b)+(a==b?18+a:0);
    if(cards::suitOf(s.hole[0])==cards::suitOf(s.hole[1]))strength+=3;
    if((a>b?a-b:b-a)==1)strength+=3;
  } else {
    uint8_t h[7]={s.hole[0],s.hole[1],kNone,kNone,kNone,kNone,kNone};
    for(int j=0;j<g.boardCount;++j)h[2+j]=g.board[j];
    const uint32_t rank=evaluate(h,2+g.boardCount);
    strength=static_cast<int>(rank>>20)*9 + (static_cast<int>((rank>>16)&15));
    if(g.boardCount==3 && (strength<14))strength+=2;
  }
  const int profile=i%3;
  const uint32_t r=random(g)%100;
  const uint32_t call=toCall(g,i);
  const uint32_t risk=call*100/(s.stack+call+1);
  if(call>0 && strength+static_cast<int>(r%18)<(profile==0?21:profile==1?17:12)+static_cast<int>(risk/4)) {
    d.action=Action::Fold;return d;
  }
  const bool strong=strength>(g.boardCount?27:profile==0?25:profile==1?20:16);
  if(strong && mayRaise(g,i) && r<(profile==0?25u:profile==1?40u:60u)) {
    uint32_t target=g.currentBet+(pot(g)/2>g.bigBlind?pot(g)/2:g.bigBlind);
    const uint32_t minimum=minRaiseTo(g),maximum=maxRaiseTo(g,i);
    if(target<minimum)target=minimum;
    if(target>maximum)target=maximum;
    if(target>g.currentBet && (target>=minimum || target==maximum)) {
      d.action=Action::RaiseTo;d.raiseTo=target;
    }
  }
  return d;
}

} // namespace poker

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/crypto.hpp>
#include <eosiolib/fixed_bytes.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/symbol.hpp>
#include <eosiolib/time.hpp>
#include <string>
#include <vector>

using namespace eosio;
using namespace std;

class [[eosio::contract]] mines : public contract {
   public:
      using contract::contract;
      mines(name receiver, name code,  datastream<const char*> ds): contract(receiver, code, ds), games(receiver, code.value) {}

      [[eosio::action]]
      void newgame(name player, asset bet_amt, char numBombs, checksum256 serverSeed_hash){
         require_auth(player);
         eosio_assert(bet_amt.is_valid(), "Invalid asset name input.");
         eosio_assert(bet_amt.symbol.precision() == 4, "Asset symbol precision must be 4.");
         eosio_assert(!(numBombs != 1 && numBombs != 3 && numBombs !=5 && numBombs !=24), "Invalid number of bombs for this game. Must be 1, 3, 5 or 24.");

         auto game_itr = games.find(player.value);
         // if previous session exists, erase it (Every player account can only have 1 game session at a time.):
         if (game_itr != games.end()){
            games.erase(game_itr);
            //todo: refund player's funds from previous session.
            print("previous removed");
         }else{
            games.emplace(_self, [&]( auto &row ) {
               row.player = player;
               row.bet_amt = bet_amt.amount;
               row.bet_symbol = bet_amt.symbol.code();
               row.numBombs = numBombs;
               row.serverSeed_hash = serverSeed_hash;
               row.bet_time = time_point_sec(now());
            });
            print("New session created");
         }
      };
      [[eosio::action]]
      void removegame(name player){
         auto game_itr = games.find(player.value);
         games.erase(game_itr);
         print("removed from table!!");
      };
      [[eosio::action]]
      void revealtile(uint8_t tile, name player){
         require_auth(player);
         eosio_assert(!(tile < 0 || tile > 24), "Invalid tile number, must be an integer from 0 to 24 (both included).");
         auto game_itr = games.find(player.value);
         eosio_assert(game_itr != games.end(), "No active game session found for that account.");
         eosio_assert(!wasRevealedBefore(tile, game_itr->revealedTiles), "That tile has been revealed already.");

         games.modify(game_itr, _self, [&]( auto &row ) {
            row.revealedTiles.push_back(tile);
            row.last_reveal_time = time_point_sec(now());
         });
         print("tile successfully revealed!");
      };
      [[eosio::action]]
      void resolvegame( name player, checksum256 serverSeed ) {
         require_auth(_self);
         //generate random bomb positions and compare it to revealed tiles.
         auto game_itr = games.find(player.value);
         eosio_assert(game_itr != games.end(), "No active game session found for that account.");

         //assert_sha256(const char * data, uint32_t length, game_itr->serverSeed_hash);
         print("this is size of a sha256 hash: ", serverSeed.size());

         std::vector<uint8_t> revTiles = game_itr->revealedTiles;
         for(uint8_t i=0; i < revTiles.size(); i++){
            print("-.-",revTiles[i]);
         }
         
         print(" --sequence of numbers!: ");
         vector<uint8_t> bombsPos;
         //Linear Congruential Generator to generate bomb positions:
         uint128_t z = serverSeed.get_array()[0]+serverSeed.get_array()[1];
         for (uint8_t i = 0; i < game_itr->numBombs; i++) {
            uint128_t m = 25, a = 11, c = 17;
            z = (a * z + c) % m;
            bombsPos.push_back(z);
            eosio::print(z, " ");
         }
         if(hasFoundBomb(revTiles, bombsPos)){
            print("BOMB HAS BEEN FOUND");
         }else{
            //pay 
         }
      };
      [[eosio::action]]
      void refundbet(name player){
         auto game_itr = games.find(player.value);
         eosio_assert(game_itr != games.end(), "No active game session found for that account.");
         time_point_sec last_reveal_time = game_itr->last_reveal_time;
         eosio_assert(time_point_sec(now() - 600) > last_reveal_time, "Wait 10 minutes");
         games.erase(game_itr);
      };
      [[eosio::action]]
      void checkresult( string clientSeed, string serverSeed, char numBombs ) {
         eosio_assert(!(numBombs != 1 && numBombs != 3 && numBombs !=5 && numBombs !=24), "Invalid number of bombs for this game. Must be 1, 3, 5 or 24.");
         //user can check their result by themselves.
         eosio::print("HELLO!!");
      };
   private:
      struct [[eosio::table]] game {
         name player;
         int64_t bet_amt;
         symbol_code bet_symbol;
         char numBombs;
         checksum256 serverSeed_hash;
         vector<uint8_t> revealedTiles;
         time_point_sec bet_time;
         time_point_sec last_reveal_time;

         uint64_t primary_key() const { return player.value; };
         EOSLIB_SERIALIZE( game, (player)(bet_amt)(bet_symbol)(numBombs)(serverSeed_hash)(revealedTiles)(bet_time)(last_reveal_time));
      };
      typedef eosio::multi_index< "game"_n, game > game_index;
      game_index games;

      //check probable fairness:
      bool wasProbablyFair(checksum256 serverSeed, char numBombs){
         return false;
      };
      bool wasRevealedBefore(uint8_t tile, vector<uint8_t> revTiles){
         for(uint8_t i = 0; i < revTiles.size(); i++){
            if(revTiles[i] == tile){
               return true;
            }
         }
         return false;
      };
      bool hasFoundBomb(vector<uint8_t> revTiles, vector<uint8_t> bombsPos){
         for(uint8_t i = 0; i < revTiles.size(); i++){
            for(uint8_t j=0; j < bombsPos.size(); j++){
               if(revTiles[i]==bombsPos[j]){
                  return true;
               }
            }
         }
         return false;
      };
};


EOSIO_DISPATCH( mines, (newgame)(resolvegame)(checkresult)(revealtile)(removegame)(refundbet))
#include <eosio/eosio.hpp>
#include <eosio/symbol.hpp>
#include <eosio/singleton.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/asset.hpp>
#include <math.h>

using namespace eosio;
using std::string;

CONTRACT hagglexstake : public contract {
   public:
      using contract::contract;

      struct [[ eosio::table, eosio::contract("hagglexstake") ]] config
      {
         // a general purpose settings map
         std::map<name, uint8_t>     settings;


         name                    staking_token_contract     = "hagglexstake"_n;
         symbol                  staking_token_symbol       = symbol ("HAG", 4);

         name                    interest_token_contract    = "hagglextoken"_n;
         symbol                  interest_token_symbol      = symbol ("HAG", 4);

         // if staking HAG and paying HAG, this would be the price of HAG.
         float                   staking_token_to_interest_token_price        = 0;
      };
 
      typedef singleton<"configs"_n, config> config_table;
      // this config placeholder makes it easier to query parameters (bug in EOSIO?)
      typedef multi_index<"configs"_n, config> config_placeholder;

      struct [[ eosio::table, eosio::contract("hagglexstake") ]] Position
      {
         uint64_t                position_id                         ;
         name                    position_owner                      ;
         asset                   staked_asset                        ;
         float                   interest_rate                       ;

         asset                   interest_paid                       ;         
         time_point_sec          last_interest_paid_time             ;

         time_point_sec          position_staked_time                = time_point_sec(current_time_point());
         time_point_sec          position_expiration_time            ;

         uint64_t                three_stakers  = 0                       ;
         uint64_t                six_stakers    = 0                      ;
         uint64_t                twelve_stakers = 0                      ;

         uint64_t                primary_key () const { return position_id; }
         uint64_t                by_owner () const { return position_owner.value; }
         uint64_t                by_amount () const { return staked_asset.amount; }
         uint64_t                by_staked_time () const { return position_staked_time.sec_since_epoch(); }
         uint64_t                by_expiration_time () const { return position_expiration_time.sec_since_epoch(); }
         uint64_t                by_duration () const { return position_expiration_time.sec_since_epoch() - 
                                                               position_staked_time.sec_since_epoch(); }
         uint64_t                by_rate () const { return (uint64_t) interest_rate * 1000000000; }
      };

      typedef multi_index<"positions"_n, Position,
         indexed_by<"byowner"_n, const_mem_fun<Position, uint64_t, &Position::by_owner>>,
         indexed_by<"byamount"_n, const_mem_fun<Position, uint64_t, &Position::by_amount>>,
         indexed_by<"bystakedtime"_n, const_mem_fun<Position, uint64_t, &Position::by_staked_time>>,
         indexed_by<"byexptime"_n, const_mem_fun<Position, uint64_t, &Position::by_expiration_time>>,
         indexed_by<"byduration"_n, const_mem_fun<Position, uint64_t, &Position::by_duration>>,
         indexed_by<"byrate"_n, const_mem_fun<Position, uint64_t, &Position::by_rate>>
      > position_table;

      struct [[ eosio::table, eosio::contract("hagglexstake") ]] balance 
      {
         asset                   funds                      ;
         name                    token_contract             ;

         uint64_t primary_key() const { return funds.symbol.code().raw(); }
      };

      typedef multi_index<"balances"_n, balance> balance_table;

            
      ACTION setprice (const float& staking_token_to_interest_token_price);

      ACTION setconfig (const name& staking_token_contract, const symbol& staking_token_symbol,
                        const name& interest_token_contract, const symbol& interest_token_symbol);
      
      ACTION setsetting ( const name& setting_name, const uint8_t& setting_value );
      ACTION pause ();
      ACTION activate ();

      ACTION stake (const name& account, const asset& quantity, const uint16_t& stake_duration_days);
      ACTION unstake (const uint64_t& position_id);
      ACTION claim (const uint64_t& position_id);
      ACTION claimall (const name& account);

      ACTION rewind (const uint64_t& position_id, const uint32_t& rewind_days);

      [[eosio::on_notify("*::transfer")]]
      void deposit ( const name& from, const name& to, const asset& quantity, const string& memo );
      void withdraw (const name& position_owner, const asset& quantity);
      ACTION withdrawall (const name& position_owner);

   private:
      const uint64_t SCALER   = 1000000;
      const uint64_t SECONDS_PER_YEAR  =  (uint64_t) 365 * 24 * 60 * 60;

      const uint16_t THREE_MONTHS = 90;
      const uint16_t SIX_MONTHS = 180;
      const uint16_t TWELVE_MONTHS = 360;

      const uint16_t THREE_MONTHS_INTEREST = 15;
      const uint16_t SIX_MONTHS_INTEREST = 30;
      const uint16_t TWELVE_MONTHS_INTEREST = 55;




      asset adjust_asset(const asset &original_asset, const float &adjustment)
      {
         return asset{static_cast<int64_t>(original_asset.amount * adjustment), original_asset.symbol};
      }

      asset get_staked_balance (const name& account) {
         position_table p_t (get_self(), get_self().value);
         auto owner_index = p_t.get_index<"byowner"_n>();
         auto owner_itr = owner_index.find (account.value);
         
         config_table      config_s (get_self(), get_self().value);
         config c = config_s.get_or_create (get_self(), config());
         asset staked_balance { 0, c.staking_token_symbol };

         while (owner_itr != owner_index.end() && owner_itr->position_owner == account) {
            staked_balance += owner_itr->staked_asset;
            owner_itr++;
         }

         return staked_balance;
      }

      asset get_available_balance (const name& account) {

         config_table      config_s (get_self(), get_self().value);
         config c = config_s.get_or_create (get_self(), config());

         balance_table balances(get_self(), account.value);
         auto it = balances.find(c.staking_token_symbol.code().raw());

         return it->funds - get_staked_balance (account);
      }

      uint8_t get_setting (const name& setting) {
         config_table      config_s (get_self(), get_self().value);
         config c = config_s.get_or_create (get_self(), config());

         return c.settings[setting];
      }

      bool is_paused () {
         return get_setting("active"_n) == 0;
      }
};
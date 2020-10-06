#include <hagglexstake.hpp>


void hagglexstake::setprice (const float& staking_token_to_interest_token_price) {
   require_auth (get_self());

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());
   c.staking_token_to_interest_token_price = staking_token_to_interest_token_price;
   config_s.set(c, get_self());
}



void hagglexstake::setconfig (const name& staking_token_contract, const symbol& staking_token_symbol,
                        const name& interest_token_contract, const symbol& interest_token_symbol) {

   require_auth (get_self());

   check (is_account(staking_token_contract), "Staking token contract " + 
      staking_token_contract.to_string() + " is not a valid account.");

   check (is_account(interest_token_contract), "Interest token contract " + 
      interest_token_contract.to_string() + " is not a valid account.");

   check (staking_token_symbol.is_valid(), "Staking token symbol " + 
      staking_token_symbol.code().to_string() + " is not a valid symbol.");

   check (interest_token_symbol.is_valid(), "Interest token symbol " + 
      interest_token_symbol.code().to_string() + " is not a valid symbol.");

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());
   c.staking_token_contract = staking_token_contract;
   c.staking_token_symbol = staking_token_symbol;
   c.interest_token_contract = interest_token_contract;
   c.interest_token_symbol = interest_token_symbol;
   config_s.set(c, get_self());
}

void hagglexstake::deposit (const name& from, const name& to, const asset& quantity, const string& memo) {

   if (to != get_self()) { return; }
   if (memo == "NODEPOSIT") { return; }   // use memo of NODEPOSIT to transfer without depositing

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());

   check (! is_paused(), "HaggleX Token Staking is paused. Try again later.");
   check (c.staking_token_symbol == quantity.symbol, "Only HAG tokens are allowed. You sent " +
      quantity.symbol.code().to_string() + "; Staking Token symbol: " + c.staking_token_symbol.code().to_string());

   check (c.staking_token_contract == get_first_receiver(), "Only HAG tokens are allowed. You sent from " +
      get_first_receiver().to_string() + "; Valid staking token contract: " + c.staking_token_contract.to_string());


   balance_table balances(get_self(), from.value);
   asset new_balance;
   auto it = balances.find(quantity.symbol.code().raw());
   if(it != balances.end()) {
      check (it->token_contract == get_first_receiver(), "Transfer does not match existing token contract.");
      balances.modify(it, get_self(), [&](auto& bal){
         bal.funds += quantity;
         new_balance = bal.funds;
      });
   }
   else {
      balances.emplace(get_self(), [&](auto& bal){
         bal.funds = quantity;
         bal.token_contract  = get_first_receiver();
         new_balance = quantity;
      });
   }

   print ("\n");
   print(name{from}, " deposited:       ", quantity, "\n");
   print(name{from}, " funds available: ", new_balance);
   print ("\n");
}


void hagglexstake::setsetting ( const name& setting_name, const uint8_t& setting_value ) {
   require_auth (get_self());

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());
   c.settings[setting_name] = setting_value;
   config_s.set(c, get_self());
}

void hagglexstake::pause () {
   setsetting ("active"_n, 0);
}

void hagglexstake::activate () {
   setsetting ("active"_n, 1);
}

void hagglexstake::stake (const name& account, const asset& quantity, const uint16_t& staked_duration_days) {
   
   check (! is_paused(), "HaggleX Staking contract is paused. Try again later.");

   config_table config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());
   
   //Check valid duration for staking
   check(staked_duration_days == THREE_MONTHS || staked_duration_days == SIX_MONTHS || staked_duration_days == TWELVE_MONTHS, "Can only stake for 90Days, 180Days or 360days.");

   //Calculate duration rate 
   float duration_interest_rate;
   uint64_t duration_stakers = 0; //
   if (staked_duration_days == THREE_MONTHS){
      duration_interest_rate = (float) THREE_MONTHS_INTEREST / 100;
      duration_stakers++;
   } 
   else if (staked_duration_days == SIX_MONTHS){
      duration_interest_rate = (float) SIX_MONTHS_INTEREST / 100;
      duration_stakers++;
   }
   else if (staked_duration_days == TWELVE_MONTHS){
      duration_interest_rate = (float) TWELVE_MONTHS_INTEREST / 100;
      duration_stakers++;
   } else {
      print("Invalid Duration");
      return;
   }
   print ("Duration interest rate    : ", std::to_string(duration_interest_rate), "\n");


   

   //Calculate size rate 
/* auto asset_amount = (float) quantity.amount / (float) pow (10, quantity.symbol.precision());
   auto size_interest_rate = (float) max_vs_constant / (float) 100;
   print ("Size interest rate    : ", std::to_string(size_interest_rate), "\n");   

   auto interest_amount = asset_amount * c.staking_token_to_interest_token_price;
   auto interest_asset  = asset { static_cast<int64_t>(interest_amount * pow (10, c.interest_token_symbol.precision())), c.interest_token_symbol };

   */

   //Populate the position table

   position_table p_t (get_self(), get_self().value);
   p_t.emplace (get_self(), [&](auto &p) {
      p.position_id                          = p_t.available_primary_key();
      p.position_owner                       = account;
      p.staked_asset                         = quantity;
      p.position_expiration_time             = time_point_sec(current_time_point().sec_since_epoch() + staked_duration_days * 24 * 60 * 60);
      p.interest_rate                        = duration_interest_rate;
      p.interest_paid                        = asset { 0, c.interest_token_symbol };

      p.three_stakers                        += duration_stakers;
      p.six_stakers                          += duration_stakers;
      p.twelve_stakers                       += duration_stakers;
   });
}


void hagglexstake::unstake (const uint64_t& position_id) {
   check (! is_paused(), "HaggleX Staking contract is paused. Try again later.");

   position_table p_t (get_self(), get_self().value);
   auto p_itr = p_t.find (position_id);
   check (p_itr != p_t.end(), "Position ID is not found: " + std::to_string(position_id));
   require_auth (p_itr->position_owner);

   // confirm that expiration date has passed
   check (current_time_point().sec_since_epoch() >= p_itr->position_expiration_time.sec_since_epoch(),
      "Cannot unstake. Staking time has not yet expired.");

   if (p_itr->last_interest_paid_time < p_itr->position_expiration_time) {
      claim (position_id);
   }

   p_t.erase (p_itr);
}



//Withdraw specific amount of tokens from the account after Staking period is over. 
void hagglexstake::withdraw (const name& position_owner, const asset& quantity) {
   require_auth (position_owner);
   check (!is_paused(), "HaggleX Staking contract is paused. Try again later.");   

   asset available_balance = get_available_balance (position_owner);
   check (available_balance >= quantity, "Insufficient funds. You requested " +
      quantity.to_string() + " but your available balance is only " + available_balance.to_string());

   balance_table balances(get_self(), position_owner.value);
   auto it = balances.find(quantity.symbol.code().raw());
   balances.modify(it, get_self(), [&](auto& bal){
      bal.funds -= quantity;
   });

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());

   string send_memo { "Withdrawal from hagglexstake" };
   action(
      permission_level{get_self(), "active"_n},
      c.interest_token_contract, 
      "transfer"_n,
      std::make_tuple(get_self(), position_owner, quantity, send_memo))
   .send();
}




void hagglexstake::withdrawall (const name& position_owner) {
   require_auth (position_owner);
   withdraw (position_owner, get_available_balance (position_owner));
}




void hagglexstake::claim (const uint64_t& position_id) {
   check (! is_paused(), "HaggleX Staking token contract is paused. Try again later.");
   position_table p_t (get_self(), get_self().value);
   auto p_itr = p_t.find (position_id);
   check (p_itr != p_t.end(), "Position ID is not found: " + std::to_string(position_id));
   require_auth (p_itr->position_owner);

   // confirm that there is interest left to be paid
   check (p_itr->last_interest_paid_time < p_itr->position_expiration_time,
      "Nothing to do. Position has expired and all interest has been claimed. You should unstake it. Position #" +
      std::to_string(position_id));

   // seconds since interest was last claimed

   uint64_t duration = (p_itr->position_expiration_time.sec_since_epoch() - p_itr->position_staked_time.sec_since_epoch()) / (60 * 60 * 24);
   auto interest_to_pay;

   if(duration >= THREE_MONTHS){
      auto duration_index = p_t.get_index<"byduration"_n>();
      auto duration_itr = duration_index.find(duration);
      asset total_staked;
      
      while (duration_itr != duration_index.end()) {
         total_staked += duration_itr=>staked_asset;
         duration_itr++;
      }

      interest_to_pay = 

   }

 


   p_t.modify (p_itr, get_self(), [&](auto &p) {
      p.interest_paid += interest_to_pay;
      p.last_interest_paid_time = time_point_sec(current_time_point());
   });

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());

   string send_memo { "Interest Payment from Position #" + std::to_string(position_id) };
   action(
      permission_level{get_self(), "active"_n},
      c.interest_token_contract, 
      "transfer"_n,
      std::make_tuple(get_self(), p_itr->position_owner, interest_to_pay, send_memo))
   .send();
}




void hagglexstake::claimall (const name& account) {
   require_auth (account);
   check (! is_paused(), "HaggleX Staking contract is paused. Try again later.");

   position_table p_t (get_self(), get_self().value);
   auto owner_index = p_t.get_index<"byowner"_n>();
   auto owner_itr = owner_index.find (account.value);

   while (owner_itr != owner_index.end() && owner_itr->position_owner == account) {
      claim (owner_itr->position_id);
      owner_itr++;
   }
}




void hagglexstake::rewind (const uint64_t& position_id, const uint32_t& rewind_days) {
   position_table p_t (get_self(), get_self().value);
   auto p_itr = p_t.find (position_id);
   check (p_itr != p_t.end(), "Position ID is not found: " + std::to_string(position_id));
   require_auth (get_self());

   p_t.modify (p_itr, get_self(), [&](auto &p) {
      p.position_staked_time -= (60 * 60 * 24 * rewind_days);
   });
}
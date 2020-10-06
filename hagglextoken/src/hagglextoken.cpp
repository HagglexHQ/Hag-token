//#define FUTURE_SUPPLY 9000000;

#include <hagglextoken/hagglextoken.hpp>

using namespace eosio;

void hagglextoken::create( const name&   issuer,
                    const asset&  maximum_supply ){
    require_auth( get_self() );

    auto sym = maximum_supply.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( maximum_supply.is_valid(), "invalid supply");
    check( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing == statstable.end(), "token with symbol already exists" );

     statstable.emplace( get_self(), [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
       s.starttime     = current_time_point().sec_since_epoch();
       s.minetime      = 0;
    });

}


void hagglextoken::issue( const name& to, const asset& quantity, const string& memo ) {
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;
    check( to == st.issuer, "tokens can only be issued to issuer account" );

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must issue positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
       s.minetime = current_time_point().sec_since_epoch();
    });

    add_balance( st.issuer, quantity, st.issuer );

}


void hagglextoken::burn( const asset& quantity, const string& memo ) {
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must burn positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

   /* statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    }); */

    sub_balance( st.issuer, quantity );
}


void hagglextoken::transfer( const name&    from,
                      const name&    to,
                      const asset&   quantity,
                      const string&  memo ) {


    blacklist_t _blacklist(get_self(), get_self().value);
    auto fromexisting = _blacklist.find( from.value );
    check( fromexisting == _blacklist.end(), "account blacklisted(from)" );
    auto toexisting = _blacklist.find( to.value );
    check( toexisting == _blacklist.end(), "account blacklisted(to)" );

    check( from != to, "cannot transfer to self" );
    require_auth( from );
    check( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );
   
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}


void hagglextoken::sub_balance( const name& owner, const asset& value ) {
   accounts from_acnts( get_self(), owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   check( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
}

void hagglextoken::add_balance( const name& owner, const asset& value, const name& ram_payer ) {
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void hagglextoken::open( const name& owner, const symbol& symbol, const name& ram_payer ) {
   require_auth( ram_payer );

   check( is_account( owner ), "owner account does not exist" );

   auto sym_code_raw = symbol.code().raw();
   print (sym_code_raw);
   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   check( st.supply.symbol == symbol, "symbol precision mismatch" );

   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
      });
   }
}


void hagglextoken::close( const name& owner, const symbol& symbol ) {
   require_auth( owner );
   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( symbol.code().raw() );
   check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}



void hagglextoken::blacklist( const name& account, const string& memo ) {
    require_auth( name("hagglexsale") );
    check( memo.size() <= 256, "memo has more than 256 bytes" );
    
    blacklist_t _blacklist( get_self(), get_self().value);
    auto existing = _blacklist.find( account.value );
    check( existing == _blacklist.end(), "blacklist account already exists" );

    _blacklist.emplace( get_self(), [&]( auto& b ) {
       b.account = account;
    });
}



void hagglextoken::unblacklist( const name& account) {
    require_auth( name("hagglexsale") );

    blacklist_t _blacklist( get_self(), get_self().value);
    auto existing = _blacklist.find( account.value );
    check( existing != _blacklist.end(), "blacklist account not exists" );

    _blacklist.erase(existing);
}



void hagglextoken::clrblacklist() {
  require_auth( name("hagglexsale") );

  blacklist_t _blacklist(get_self(), get_self().value);

  // Delete all records in _messages table
  auto list_itr = _blacklist.begin();
  while (list_itr != _blacklist.end()) {
    list_itr = _blacklist.erase(list_itr);
  }
}



void hagglextoken::mint(const symbol_code& sym){ 
   
   //check that the symbol is valid
   check( sym.is_valid(), "invalid symbol name" );

   stats statstable( get_self(), sym.raw() );
   const auto& st = statstable.get( sym.raw() );
   
   require_auth(st.issuer);

   const uint32_t one_day = 86400;
   const uint32_t ninety_days = 90 * one_day;

   if( st.supply.amount/10000 <= 9000000 ){

      asset reward = get_reward(st.supply, sym);
      uint32_t currenttime = current_time_point().sec_since_epoch();

      if( currenttime > (st.starttime + ninety_days) ){

         if( currenttime > (st.minetime + one_day) ){

            action{
                  permission_level{get_self(), "active"_n}, 
                  get_self(),
                  "issue"_n,
                  std::make_tuple(get_self(), reward * 10000, std::string("Issue tokens"))
               }.send(); 
            }
         } 
      }//if
}








EOSIO_DISPATCH( hagglextoken, (create)(issue)(transfer)(burn)(open)(close)(mint)(blacklist)(unblacklist)(clrblacklist))

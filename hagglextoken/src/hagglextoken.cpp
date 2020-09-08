//#define FUTURE_SUPPLY 9000000;

#include <hagglextoken/hagglextoken.hpp>

using namespace eosio;

void hagglextoken::create( const name&   issuer,
                    const asset&  maximum_supply )
{
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


void hagglextoken::issue( const name& to, const asset& quantity, const string& memo )
{
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
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
       s.minetime = current_time_point().sec_since_epoch();
    });

    add_balance( st.issuer, quantity, st.issuer );

}

void hagglextoken::retire( const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must retire positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

   /* statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    }); */

    sub_balance( st.issuer, quantity );
}

void hagglextoken::transfer( const name&    from,
                      const name&    to,
                      const asset&   quantity,
                      const string&  memo )
{

    blacklists blacklistt(get_self(), get_self().value);
    auto fromexisting = blacklistt.find( from.value );
    check( fromexisting == blacklistt.end(), "account blacklisted(from)" );
    auto toexisting = blacklistt.find( to.value );
    check( toexisting == blacklistt.end(), "account blacklisted(to)" );

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

void hagglextoken::add_balance( const name& owner, const asset& value, const name& ram_payer )
{
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

void hagglextoken::open( const name& owner, const symbol& symbol, const name& ram_payer )
{
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


void hagglextoken::close( const name& owner, const symbol& symbol )
{
   require_auth( owner );
   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( symbol.code().raw() );
   check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}




void hagglextoken::blacklist( name account, string memo ) {
    require_auth( get_self() );
    check( memo.size() <= 256, "memo has more than 256 bytes" );
    
    blacklists blacklistt( get_self(), get_self().value);
    auto existing = blacklistt.find( account.value );
    check( existing == blacklistt.end(), "blacklist account already exists" );

    blacklistt.emplace( get_self(), [&]( auto& b ) {
       b.account = account;
    });
}


void hagglextoken::unblacklist( name account) {
    require_auth( get_self() );

    blacklists blacklistt( get_self(), get_self().value);
    auto existing = blacklistt.find( account.value );
    check( existing != blacklistt.end(), "blacklist account not exists" );

    blacklistt.erase(existing);
}





void hagglextoken::mint(const symbol_code& sym){ 
   
   stats statstable( get_self(), sym.raw() );
   const auto& st = statstable.get( sym.raw() );
   
   require_auth(st.issuer);

   const uint32_t one_day = 86400;
   const uint32_t ninety_days = 90 * one_day;

   if( st.supply.amount/10000 <= 9000000 ){

      asset reward = get_reward(st.supply, sym);
      uint32_t currenttime = current_time_point().sec_since_epoch();

      if( currenttime > (st.starttime + 600) ){

         if( currenttime > (st.minetime + 600) ){

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


EOSIO_DISPATCH( hagglextoken, (create)(issue)(transfer)(retire)(open)(mint)(close)(blacklist)(unblacklist))

#pragma once

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/time.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>

using namespace std;
using namespace eosio;

CONTRACT hagglexsale : public contract
{
  public:
    using contract::contract;
    // constructor
    hagglexsale(name receiver, name code, datastream<const char *> ds) : 
        contract(receiver, code, ds),
        state_singleton(get_self(), get_self().value), // code and scope both set to the contract's account
        reserved_singleton(get_self(), get_self().value),
        state(state_singleton.get_or_default(default_state())),
        reserved(reserved_singleton.get_or_default(default_reserved()))        
        //state(state_singleton.exists() ? state_singleton.get() : default_state()), // get the singleton if it exists already
        //reserved(reserved_singleton.exists() ? reserved_singleton.get() : default_reserved())
        {}

    // destructor
    ~hagglexsale() 
    {
        state_singleton.set(state, get_self()); // persist the state of the crowdsale before destroying instance

        print("\nSaving state to the RAM");
        print(state.toString());

        reserved_singleton.set(reserved, get_self()); // persist the state of the crowdsale before destroying instance

        print("Saving state to the RAM ");
        print(reserved.toString());   
    }

    ACTION init(const name& admin, const eosio::time_point_sec& start, const eosio::time_point_sec& finish); // initialize the crowdsale

    // to hagglexsale
    [[eosio::on_notify("*::transfer")]]
    void buyhagglex(const name& from, const name& to, asset& quantity, const string& memo); // redirect to handle_investment

    ACTION issue(const name& to, asset& quantity, const uint64_t& _class, const std::string& memo);

    ACTION withdraw(const symbol_code& sym); // transfer tokens from the contract account to the recipient

    ACTION pause(); // for pause/unpause contract

    ACTION finalize(); // for unlocking the transfer of tokens after ICO sale
    

    
    

  private:

    const symbol sy_hag = symbol("HAG", 4);
    const symbol sy_eos = symbol("EOS", 4);
    const symbol sy_voice = symbol("VOICE", 4);

    const asset zero_hag = asset(0, symbol("HAG", 4 )); 
    const asset zero_eos = asset(0, symbol("EOS", 4 )); 
    const asset zero_voice = asset(0, symbol("VOICE", 4 )); 

    


    // type for defining state
    struct state_t
    {
        name                admin;
        uint64_t            total_hag_tokens;
        uint64_t            total_eos_tokens;
        uint64_t            total_voice_tokens;
        uint64_t            total_eosio_tokens;
        time_point_sec      start;
        time_point_sec      finish;
        bool                pause;

        // utility method for converting this object to string
        string toString()
        {
            string str =    " RECIPIENT " + admin.to_string() +
                            " \nPAUSED " + std::to_string(pause) +
                            " \nHAG TOKENS " + std::to_string(total_hag_tokens/10000) +
                            " \nALL EOSIO TOKENS " + std::to_string(total_eosio_tokens/10000) +
                            " \nSTART " + std::to_string(start.utc_seconds) +
                            " \nFINISH " + std::to_string(finish.utc_seconds);

        return str;
        }
    };

    // type for issuing the reserved tokens
    struct reserved_t
    {
        asset class1;
        asset class2;
        asset class3;
        asset class4;
        asset class5;
        asset class6;
        asset class7;
        asset class8;

        // utility method for converting this object to string
        std::string toString()
        {
            std::string str = " Core Team "  + std::to_string(class1.amount/10000)  +
                              " Advisors " +  std::to_string(class2.amount/10000)  +
                              " Core Investors " + std::to_string(class3.amount/10000) +
                              " Reserved " +  std::to_string(class4.amount/10000)  +
                              " ICO " + std::to_string(class5.amount) +
                              " Charity " + std::to_string(class6.amount/10000) +
                              " Founding Team " + std::to_string(class7.amount/10000) +
                              " Airgrab " + std::to_string(class8.amount/10000);

            return str;
        }
    };

  
    // table for holding investors information
    TABLE deposit_t
    {
        name account;
     // asset eoses;
        asset tokens;
        uint64_t primary_key() const { return account.value; }
    };

    // persists the state of the aplication in a singleton. Only one instance will be strored in the RAM for this application
    eosio::singleton<"state"_n, state_t> state_singleton;

        // persists the state of reserved tokens 
    eosio::singleton<"reserved"_n, reserved_t> reserved_singleton;

    // store investors and balances with contributions in the RAM
    typedef eosio::multi_index<"deposit"_n, deposit_t> deposits;

   // hold present state of the application
    state_t state;

    // holds reserved tokens state for all classes
    reserved_t reserved;

    // a utility function to return default parameters for the state of the crowdsale
    state_t default_state() const
    {
        state_t ret;
        
        ret.total_hag_tokens = 0;   
        ret.total_eosio_tokens = 0;    
        ret.pause = false;
        ret.start = time_point_sec(0);
        ret.finish = time_point_sec(0);

        return ret;
    }

    // a utility function to return default parameters for the state of the crowdsale
    reserved_t default_reserved() const
    {
        reserved_t res;
        res.class1 = zero_hag;
        res.class2 = zero_hag;
        res.class3 = zero_hag;
        res.class4 = zero_hag;
        res.class5 = zero_hag;
        res.class6 = zero_hag;
        res.class7 = zero_hag;
        res.class8 = zero_hag;
        
        return res;
    }

    // handle transfer of tokens
    void inline_transfer(const name& from, const name& to, asset& quantity, const string& memo){
        action(
            eosio::permission_level(get_self(), "active"_n),
            name("hagglextoken"),
            name("transfer"),
            make_tuple(from, to, quantity, name{to}.to_string() +  memo)
        ).send();
    }

    // handle blacklisting of accounts
    void inline_blacklist(const name& investor, const string& memo) {
        action(
            eosio::permission_level(get_self(), "active"_n),
            name("hagglextoken"),
            name("blacklist"),
            make_tuple(investor, memo)
        ).send();
    }

    // handle unblacklisting of accounts
    void inline_unblacklist(const name& investor) {
        action(
            eosio::permission_level(get_self(), "active"_n),
            name("hagglextoken"),
            name("unblacklist"),
            make_tuple(investor)
        ).send();
    }


    //clear blacklist 
    void inline_clrblacklist() {
        action(
            eosio::permission_level(get_self(), "active"_n),
            name("hagglextoken"),
            name("clrblacklist"),
            make_tuple("")
        ).send();
    }


    // update contract balances and send HAG tokens to the investor
    void handle_investment(const name& investor, const uint64_t& tokens_to_give){   
        deposits _deposit(get_self(), get_self().value);
        auto it = _deposit.find(investor.value);

        // if the depositor account was found, store his updated balance
        asset entire_tokens = asset(tokens_to_give, symbol("HAG", 4));

        // if the depositor was not found create a new entry in the database, else update his balance
            if (it == _deposit.end())
            {
                _deposit.emplace(get_self(), [investor, entire_tokens](auto &deposit) {
                    deposit.account = investor;
                    deposit.tokens = entire_tokens;
                });
            }
            else
            {
                _deposit.modify(it, get_self(), [investor, entire_tokens](auto &deposit) {
                    deposit.tokens += entire_tokens;
                });
            }

        //Blacklist the account not to transfer the tokens at first
        inline_blacklist(investor, "ICO Sale");
    }

};
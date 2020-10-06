#include <hagglexsale.hpp>
#include <config.h>                        

// initialize the crowdfund
ACTION hagglexsale::init(const name& admin, const time_point_sec& start, const time_point_sec& finish)
{
    check(!state_singleton.exists(), "Already Initialzed");
    check(start < finish, "Start must be less than finish");
    require_auth(get_self());
    check(admin != get_self(), "Admin should be different than contract deployer");

    // update state
    state.admin = admin;
    state.start = start;
    state.finish = finish;
    state.pause = false;

    //Update ICO Reserve(Class5)
    reserved.class5.amount += CLASS5MAX/10000; 
    
    const asset goal = asset(GOAL, symbol("HAG", 4));

}


//BUY HAG TOKENS WITH EOS COINS AND STORE THEM ON THE HaggleXSale ADDRESS
// handle transfers to this contract
void hagglexsale::buyhagglex(const name& from, const name& to, asset& quantity, const string& memo)
{   
    //to ensure the conttract is not transfering to itself
    if (to != get_self() || from == get_self())
    {
        print("These are not the droids you are looking for.");
        return;
    }

    //make sure you are receiving the right coin in exchange to purchase the HAG tokens
    check(quantity.symbol == sy_eos || quantity.symbol == sy_voice, "Can only buy with EOS or VOICE on this window");


    check( quantity.is_valid(), "invalid quantity" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    check(state.pause == false, "Crowdsale has been paused" );
    
    // check timings of the HAG crowdsale
    check(current_time_point().sec_since_epoch() >= state.start.utc_seconds, "Crowdsale hasn't started");

    //check if account exists with the corresponding balance
    deposits _deposit(get_self(), get_self().value);
    auto it = _deposit.find(from.value);
    if(it != _deposit.end()) {   
        check((it->tokens.amount + quantity.amount) <= MAX_CONTRIB, "Can not purchase more than the Maximum Contribution");
        inline_unblacklist(from);
    }

    // check if the Goal was reached
    check(state.total_hag_tokens <= GOAL, "GOAL reached");

     //update the total eoses received
    state.total_eosio_tokens += quantity.amount;

    //calculate 3% fees on buying EOS or VOICE
    //calculate the amount of tokens to give and update the total tokens
        asset fees ;
        int64_t tokens_to_give;
        fees.amount = quantity.amount*0.03;
    if(quantity.symbol == sy_eos){
        fees.symbol = sy_eos;  
        print(fees);
        state.total_eos_tokens += quantity.amount;
        tokens_to_give = (quantity.amount * 3.14)/RATE;
    } else if (quantity.symbol == sy_voice){
        fees.symbol = sy_voice; 
        print(fees);
        state.total_voice_tokens += quantity.amount;
        tokens_to_give = (quantity.amount * 2.38)/RATE;
    }

    quantity-=fees;
    // dont send fees to _self
    // else HAG supply would increase
  
    //update the total HAG tokens 
    state.total_hag_tokens += tokens_to_give; 

       // check the minimum and maximum contribution
    check(tokens_to_give >= MIN_CONTRIB, "Contribution too low");
    check(tokens_to_give <= MAX_CONTRIB, "Contribution too high");

    
    //update the ICO reserve accordingly
    reserved.class5.amount -= quantity.amount;



    // set the amounts to transfer, then call inline transfer action to update balances in the token contract
    asset amount = asset(tokens_to_give, symbol("HAG", 4));

    //Finally, send the HAG tokens to the to the buyer
    inline_transfer(get_self(), from, amount, " purchased HAG tokens SUCCESSFULLY");
    
    
    //enlist investor/buyer and blacklist them
    handle_investment(from, tokens_to_give);

    print(from);
    print(to);
    print(quantity);
    print(memo);
}




// issuance of only reserved HAG tokens
ACTION hagglexsale::issue(const name& to, asset& quantity, const uint64_t& _class, const std::string& memo)
{
    require_auth(state.admin);
    check( is_account( to ), "to account does not exist" );
    check( quantity.symbol == sy_hag ,"Can issue only HAG coins");
    switch(_class){
        case 1:
        check((reserved.class1.amount + quantity.amount) <= CLASS1MAX, "Cannot issue more than Core Team quantity");
        reserved.class1 += quantity;
        break;
        case 2:
        check((reserved.class2.amount + quantity.amount) <= CLASS2MAX, "Cannot issue more than Advisors quantity");
        reserved.class2 += quantity;
        break;        
        case 3:
        check((reserved.class3.amount + quantity.amount) <= CLASS3MAX, "Cannot issue more than Core Investors quantity");
        reserved.class3 += quantity;
        break;
        case 4:
        check((reserved.class4.amount + quantity.amount) <= CLASS4MAX, "Cannot issue more than Reserved quantity");
        reserved.class4 += quantity;
        break;
        case 5:
        check((reserved.class5.amount + quantity.amount) <= CLASS5MAX, "Cannot issue more than ICO quantity");
        reserved.class5 += quantity;
        break;
        case 6:
        check((reserved.class6.amount + quantity.amount) <= CLASS6MAX, "Cannot issue more than Charity  quantity");
        reserved.class6 += quantity;
        case 7:
        check((reserved.class7.amount + quantity.amount) <= CLASS7MAX, "Cannot issue more than Founding Team quantity");
        reserved.class7 += quantity;
        break;
        case 8:
        check((reserved.class8.amount + quantity.amount) <= CLASS8MAX, "Cannot issue more than Airgrab quantity");
        reserved.class8 += quantity;
        }

    // issues HAG tokens to the beneficiary class
    inline_transfer(get_self(), to, quantity, " got Issued tokens to the Beneficiary Class SUCCESSFULLY");
    
    //enlist investor/buyer and blacklist them
    handle_investment(to, quantity.amount);
}





// used by ADMIN to withdraw EOS and VOICE tokens.
ACTION hagglexsale::withdraw(const symbol_code& sym)
{
    require_auth(state.admin);

    //make sure you are receiving the right coin in exchange to purchase the HAG tokens
    check(sym.raw() == sy_eos.code().raw() || sym.raw() == sy_voice.code().raw(), "Can only withdraw EOS or VOICE");

    check(current_time_point().sec_since_epoch() <= state.finish.utc_seconds, "Crowdsale not ended yet" );
    check(state.total_eosio_tokens <= SOFT_CAP_TKN, "Soft cap was not reached");

    
    if(sym.raw() == sy_eos.code().raw()) {
        asset all_eos = asset(state.total_eos_tokens, symbol("EOS", 4));

        //transfer all the EOS on the smart contract account to the Recepient
        inline_transfer(get_self(), state.admin, all_eos, "withdrew EOS tokens");

        //update the total EOS tokens state to 0;
        state.total_eos_tokens = 0;
    } 
    else if(sym.raw() == sy_voice.code().raw()) {
         asset all_voice = asset(state.total_eos_tokens, symbol("VOICE", 4));

        //transfer all the VOICE on the smart contract account to the Recepient
        inline_transfer(get_self(), state.admin, all_voice, " withdrew VOICE tokens");

        //update the totale VOICE tokens state to 0;
        state.total_voice_tokens = 0;
    }  

}




// toggles unpause / pause contract
ACTION hagglexsale::pause()
{
    require_auth(state.admin);
    if (state.pause == false){
        state.pause = true; 
    }
    else{
        state.pause = false;
    }
        
}



//toggles the unlock of the transfer of HAG tokens
ACTION hagglexsale::finalize() {
    require_auth(state.admin);
	//check(current_time_point().sec_since_epoch() > state.finish.utc_seconds, "Crowdsale hasn't finished");
	//check(state.total_eosio_tokens >= SOFT_CAP_TKN, "Soft cap was not reached");

    //Toggle the lock action again to FALSE after the ICO.
	inline_clrblacklist();

    // Delete all records in _messages table
    deposits _deposit(get_self(), get_self().value);
    auto itr = _deposit.begin();
    while (itr != _deposit.end()) {
        itr = _deposit.erase(itr);
  }
    
}





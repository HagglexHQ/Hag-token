/* eslint-disable prettier/prettier */
const assert = require('assert');
const eoslime = require('eoslime').init('local');

const VAULT_WASM_PATH = '../vault/vault.wasm';
const VAULT_ABI_PATH = '../vault/vault.abi';
const TOKEN_WASM_PATH = '/Users/max/dev/token/token/token.wasm';
const TOKEN_ABI_PATH = '/Users/max/dev/token/token/token.abi';

let vaultContract, tokenContract;
let vaultAccount, tokenAccount;

async function getBalance (account, index) {
    let accountTable = await tokenContract.provider.eos.getTableRows({
        code: tokenContract.name,
        scope: account.name,
        table: 'accounts',
        json: true
    });
    console.log ("\nBalance for user: ", account.name);
    console.log (accountTable.rows[index], "\n");
    return accountTable.rows[index];
}

describe('Vault Testing', function () {

    // Increase mocha(testing framework) time, otherwise tests fails
    this.timeout(150000);

    
    let user1, user2, user3, user4, user5;
    let accounts;
    let config;

    before(async () => {

        accounts = await eoslime.Account.createRandoms(20);
        vaultAccount        = accounts[0];
        tokenAccount        = accounts[1];
        
        user1               = accounts[4];
        user2               = accounts[5];
        user3               = accounts[6];
        user4               = accounts[7];
        user5               = accounts[8];
       
        console.log (" Vault Account        : ", vaultAccount.name);
        console.log (" EOS Token            : ", tokenAccount.name)
        console.log (" user1                : ", user1.name);
        console.log (" user2                : ", user2.name);
        console.log (" user3                : ", user3.name);
        console.log (" user4                : ", user4.name);
        console.log (" user5                : ", user5.name);

        const initialEOS = '1000000.0000 EOS';
        const initialGFT = '1000000.00000000 GFT';

        await vaultAccount.addPermission('eosio.code');

        vaultContract = await eoslime.AccountDeployer.deploy (VAULT_WASM_PATH, VAULT_ABI_PATH, vaultAccount);
        tokenContract = await eoslime.AccountDeployer.deploy (TOKEN_WASM_PATH, TOKEN_ABI_PATH, tokenAccount);
    
        await vaultContract.setconfig(tokenAccount.name, "4,EOS",
                tokenAccount.name, "8,GFT", { from: vaultAccount });
        await vaultContract.activate({ from: vaultAccount });
        await vaultContract.setprice (1, { from: vaultAccount });

        await tokenContract.create(tokenContract.name, '1000000000.0000 EOS');
        await tokenContract.create(tokenContract.name, '1000000000.00000000 GFT');

        console.log ("\n\n");
        console.log ("Users each receive 1000.0000 EOS at the beginning of all tests.")
        await tokenContract.issue(tokenAccount.name, "100000000.0000 EOS", 'memo', { from: tokenContract});
        await tokenContract.issue(tokenAccount.name, "100000000.00000000 GFT", 'memo', { from: tokenContract});

        await tokenContract.transfer(tokenAccount.name, user1.name, initialEOS, 'memo', { from: tokenContract});
        await tokenContract.transfer(tokenAccount.name, user2.name, initialEOS, 'memo', { from: tokenContract});
        await tokenContract.transfer(tokenAccount.name, user3.name, initialEOS, 'memo', { from: tokenContract});
        await tokenContract.transfer(tokenAccount.name, user4.name, initialEOS, 'memo', { from: tokenContract});
        await tokenContract.transfer(tokenAccount.name, user5.name, initialEOS, 'memo', { from: tokenContract});

        await tokenContract.transfer(tokenAccount.name, user1.name, initialGFT, 'memo', { from: tokenContract});
        await tokenContract.transfer(tokenAccount.name, user2.name, initialGFT, 'memo', { from: tokenContract});
        await tokenContract.transfer(tokenAccount.name, user3.name, initialGFT, 'memo', { from: tokenContract});
        await tokenContract.transfer(tokenAccount.name, user4.name, initialGFT, 'memo', { from: tokenContract});
        await tokenContract.transfer(tokenAccount.name, user5.name, initialGFT, 'memo', { from: tokenContract});

        await tokenContract.transfer(tokenAccount.name, vaultAccount.name, initialGFT, 'NODEPOSIT', { from: tokenContract});

    });

    beforeEach(async () => {
   
    });

    it('Set Vars', async () => {

        await vaultContract.setvars (2, 0.200000, 0.100000, 0.400000, { from: vaultAccount });
        
        console.log (" Here are the platform configurations.")
        let configTable = await vaultContract.provider.eos.getTableRows({
            code: vaultContract.name,
            scope: vaultContract.name,
            table: 'configs',
            json: true
        });
        config = configTable.rows[0];
        console.log (config);

        assert.equal (config.shift, 2);
        assert.equal (parseFloat(config.tilt).toFixed(6), 0.200000);
        assert.equal (parseFloat(config.butterfly).toFixed(6), 0.100000);
        assert.equal (parseFloat(config.constant).toFixed(6), 0.400000);       
    });

    it('Accepts deposits', async () => {

        await tokenContract.transfer(user1.name, vaultAccount.name, "100000.0000 EOS", "", { from: user1 });

        let userBalance = await vaultContract.provider.eos.getTableRows({
            code: tokenContract.name,
            scope: user1.name,
            table: 'accounts',
            json: true
        });
        console.log( userBalance );       
        assert.equal (userBalance.rows[0].balance, "900000.0000 EOS"); 

        userBalance = await vaultContract.provider.eos.getTableRows({
            code: vaultAccount.name,
            scope: user1.name,
            table: 'balances',
            json: true
        });
        console.log( userBalance );       
        assert.equal (userBalance.rows[0].funds, "100000.0000 EOS"); 
    });

    it('Calculates interest', async () => {

        let pos = 0;
        await vaultContract.stake(user1.name, "100.0000 EOS", 365, { from: user1 });
        let stake = await vaultContract.provider.eos.getTableRows({
            code: vaultAccount.name,
            scope: vaultAccount.name,
            table: 'positions',
            json: true
        });
        console.log( stake.rows[pos] );       
        assert.equal (parseFloat(stake.rows[pos].interest_rate).toFixed(5), 0.03100); 
        pos++;

        await vaultContract.stake(user1.name, "100.0000 EOS", 91, { from: user1 });
        stake = await vaultContract.provider.eos.getTableRows({
            code: vaultAccount.name,
            scope: vaultAccount.name,
            table: 'positions',
            json: true
        });
        console.log( stake.rows[pos] );       
        assert.equal (parseFloat(stake.rows[pos].interest_rate).toFixed(5), 0.02856); 
        pos++;

        await vaultContract.stake(user1.name, "100.0000 EOS", 183, { from: user1 });
        stake = await vaultContract.provider.eos.getTableRows({
            code: vaultAccount.name,
            scope: vaultAccount.name,
            table: 'positions',
            json: true
        });
        console.log( stake.rows[pos] );       
        assert.equal (parseFloat(stake.rows[pos].interest_rate).toFixed(5), 0.02925); 
        pos++;

        await vaultContract.stake(user1.name, "100.0000 EOS", 1825, { from: user1 });
        stake = await vaultContract.provider.eos.getTableRows({
            code: vaultAccount.name,
            scope: vaultAccount.name,
            table: 'positions',
            json: true
        });
        console.log( stake.rows[pos] );       
        assert.equal (parseFloat(stake.rows[pos].interest_rate).toFixed(5), 0.06300); 
        pos++;

        await vaultContract.stake(user1.name, "100.0000 EOS", 3650, { from: user1 });
        stake = await vaultContract.provider.eos.getTableRows({
            code: vaultAccount.name,
            scope: vaultAccount.name,
            table: 'positions',
            json: true
        });
        console.log( stake.rows[pos] );       
        assert.equal (parseFloat(stake.rows[pos].interest_rate).toFixed(5), 0.14800); 
        pos++;
    });

    it('Pays Interest', async () => {

        await getBalance (user1, 1);        
        await vaultContract.rewind (4, 93, { from: vaultAccount });
        await vaultContract.claim (4, { from: user1 });
        var gftBalance = await getBalance (user1, 1);
        var expectedStr = "1000003.77095936 GFT";
        assert.equal (gftBalance.balance.substr(0, 11), expectedStr.substr(0, 11));

        stake = await vaultContract.provider.eos.getTableRows({
            code: vaultAccount.name,
            scope: vaultAccount.name,
            table: 'positions',
            json: true
        });
        console.log( stake.rows[4] );       
    });

    it('Withdraws', async () => {

        await getBalance (user1, 0);        
        await vaultContract.withdrawall (user1.name, { from: user1 });
        var eosBalance = await getBalance (user1, 0);
        assert.equal (eosBalance.balance, '999500.0000 EOS');
   
    });


});
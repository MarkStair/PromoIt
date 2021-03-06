#include <utility>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/name.hpp>
#include <eosiolib/action.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/symbol.hpp>

using eosio::key256;
using eosio::indexed_by;
using eosio::const_mem_fun;
using eosio::asset;
using eosio::permission_level;
using eosio::action;
using eosio::datastream;
using eosio::print;
using eosio::name;
using eosio::contract;
using std::string;

CONTRACT promoteit : public eosio::contract {
    private:

        /* Rundown of structs:
        
         * Users can create PRODUCTS (kinds of items) and issue ITEMS (instances of
         * products). When a user purchases an item, half of the total goes to a
         * reward record in REWARDS. Users who have items with outstanding rewards
         * can claim their rewards, adding to CLAIMS.
         * 
         * For now, we have not implemented deposit-then-send behavior, so we handle
         * transactions in this contract via ACCOUNTS and transact()
         */

        struct products;
        struct items;
        struct rewards;
        struct claims;
        struct accounts;
        struct referrals;

    public:

    using contract::contract;
    promoteit( name self, name code, datastream<const char*> ds ):
        contract(self, code, ds),
        items(_code, _code.value),
        products(_code, _code.value),
        rewards(_code, _code.value),
        claims(_code, _code.value),
        accounts(_code, _code.value),
        referrals(_code, _code.value)
    {}


    /* createitem() lets users add new items. Ultimately this will be subject to
     * centralized approval of the item, or to some kind of consensus mechanism. In
     * addition, a good deal more information than just a "description" and "quantity"
     * can be included.
     */
    ACTION createitem ( const string description, const uint64_t quantity, const name creator ) {
        eosio::require_auth ( creator );
        eosio_assert ( quantity > 0, "You must create at least 1 of this item");
        
        auto product_itr = products.emplace( _code, [&]( auto& product ){
            product.productid	    = products.available_primary_key();
            product.description     = description;
            product.unissuedct      = quantity;
            product.creator         = creator;
        });

    }

    /* issueitem() lets creators issue items in batches. The current inelegant solution
     * to keep the action reasonably lightweight is to limit issuance to 10 at a time.
     * This posts the items for sale at the creator's set price.
     */
    ACTION issueitem ( const name creator, const uint64_t productid, const uint64_t quantity, const uint64_t listprice ) {
        eosio::require_auth ( creator );
        eosio_assert ( quantity > 0, "You must issue more than 0 items");
        eosio_assert ( quantity < 11, "Max 10 items per issue");

        string not_enough_msg = "Cannot issue this many (only " + std::to_string(quantity) + " remaining)";
        const char *msgstr = not_enough_msg.c_str();


        auto prodtoissue = products.find( productid );
        eosio_assert (!(prodtoissue == products.end()), "Product not found.");
        eosio_assert ((prodtoissue->creator == creator), "Only creator can issue items.");
        eosio_assert ((prodtoissue->unissuedct > 0), "No products left to issue.");
        eosio_assert ((prodtoissue->unissuedct > quantity), msgstr);

        uint64_t new_unissuedct = (prodtoissue->unissuedct) - quantity;
        
        for ( uint64_t qtct = 1; qtct <= quantity; qtct++ ) {
            auto item_itr = items.emplace( _code, [&]( auto& item ){
                    item.serialnum			= items.available_primary_key();
                    item.owner  	        = creator;
                    item.currentprice       = listprice;
                    item.productid          = productid;
            });
        }

        products.modify( prodtoissue, _self, [&]( auto& product ) {
                product.unissuedct      = new_unissuedct;
        });
    }

    /* For a buyer to post an item for sale on the secondary market, or for
     * creators to re-price an item, postitem()
     */
    ACTION postitem ( const uint64_t serialnum, const name seller, const uint64_t pricetopost ) {
        eosio::require_auth ( seller );
        auto goodtopost = items.find( serialnum );
        eosio_assert (!(goodtopost == items.end()), "Item not found.");

        items.modify( goodtopost, _self, [&]( auto& item ) {
            item.currentprice   = pricetopost;
        });

    }

    /* Buying items not only sends money from buyer to seller and transfers
     * ownership of the item, it also creates a claimable reward for
     * the item with rewardowners()
     */
    ACTION buyitem ( const uint64_t serialnum, const string buyername ) {
        name buyer = name(buyername);
        eosio::require_auth ( buyer );
        auto goodtosell = items.find( serialnum );
        eosio_assert (!(goodtosell == items.end()), "Item not found.");
        eosio_assert (!(goodtosell->currentprice == 0), "Item not for sale.");

        string memo = "Purchasing item " + std::to_string(serialnum);

        // we're temporarily using a local accounts table and transact(),
        // but would implement a deposit-then-spend structure

        // eosio::asset tosend = eosio::asset(goodtosell->currentprice/2, eosio::symbol(eosio::symbol_code("PROMO"),4));
        double tosend = (goodtosell->currentprice/2);
        transact( buyer, goodtosell->owner, tosend );

        rewardowners ( goodtosell->productid, goodtosell->owner, (goodtosell->currentprice/2) );

        items.modify( goodtosell, get_self(), [&]( auto& item ) {
            item.owner          = buyer;
            item.currentprice   = 0;
        });
    }

    /* Rewards are not automatic; this would create a large number of transactions.
     * Instead, the client app can findrewards() and notify the user, who can then
     * claimreward()
     */
    ACTION claimreward ( const name claimer, const uint64_t serialnum, const uint64_t rewardid ) {
        eosio::require_auth ( claimer );
        auto reward_toclaim = rewards.find( rewardid );

        // for time, shortcutting this to easily find "has user x claimed reward y?"
        uint128_t easylookr = std::stoi(std::to_string(claimer.value) + std::to_string(rewardid));

        auto claim_search = claims.get_index<name("easyfindr")>();
        auto claim_find = claim_search.find( easylookr );
        eosio_assert ((claim_find == claim_search.end()),"You've already claimed this reward!");

        uint64_t owned_count = 0;

        auto items_search = items.get_index<name("productid")>();
        for ( auto items_owned = items_search.begin(); items_owned != items_search.end(); items_owned++ ) {
            if ( items_owned->owner == claimer ) {
                owned_count++;
            }
        }

        auto claim_itr = claims.emplace( _code, [&]( auto& claim ){
            claim.id			= claims.available_primary_key();
            claim.claimer       = claimer;
            claim.rewardid      = rewardid;
            claim.easyfindr     = easylookr;
        });

        // temporarily using local accts table and transact(). Obviously reward poster's authorization
        // wouldn't be here; instead reward would be held in contract and dispersed

        // eosio::asset tosend = eosio::asset(reward_toclaim->totalamt/reward_toclaim->totalrewards, eosio::symbol(eosio::symbol_code("PROMO"),4));

        double tosend = (reward_toclaim->totalamt/reward_toclaim->totalrewards);
        transact( reward_toclaim->poster, claimer, tosend );

    }

    /* Rewards are not automatic; this would create a large number of transactions.
     * Instead, the client app can findrewards() and notify the user, who can then
     * claimreward(). findrewards() hasn't been implemented yet; a client could at
     * this point use a chain of cleos get table calls to find the needed
     * rewardids for claimreward().
     */
    ACTION findrewards ( const name owner ) {
        
        // Not yet implemented.
        // This will return all rewards owner has waiting to be claimed
        // reward_index rewards holds pending rewards to be claimed,
        // so user can find rewards for any items the user holds
        
    }

    /* This contract must implement a deposit-then-spend model for transactions,
     * but for now it uses its own accounts struct and a transact() function.
     */
    ACTION createacct ( const name accountname, uint64_t assetamount ) {
        require_auth(get_self());
        auto acct_itr = accounts.emplace( _code, [&]( auto& account ){
            account.id              = accounts.available_primary_key();
            account.owner   	    = accountname;
            account.token_balance   = assetamount;
        });
    }
    
    /* XXXXXXXXXXXXXXXXXXXXXXXXXXXX
     * For testing only, to clear all tables. Do not deploy.
     */
    ACTION cleartables( const uint64_t nonsense ) {
        require_auth ( _self );

        auto it = items.begin();
        auto pt = products.begin();
        auto rwd = rewards.begin();
        auto clm = claims.begin();
        auto acct = accounts.begin();
        
        while (it != items.end()) { it = items.erase(it); }
        while (pt != products.end()) { pt = products.erase(pt); }    
        while (rwd != rewards.end()) { rwd = rewards.erase(rwd); }      
        while (clm != claims.end()) { clm = claims.erase(clm); } 
        while (acct != accounts.end()) { acct = accounts.erase(acct); }
    } // For testing only.


    /* Not implemented yet. A referral system option would allow developers
     * to change from the model of "everyone who owns gets a %" to a
     * model where ENCOUNTERS (as determined by the developer) with a
     * player wearing an item count as a referral
     */
    ACTION referitem ( const uint64_t serialnum, const name potentlbuyer ) {

        auto referralgood = items.find( serialnum );
        eosio_assert (!(referralgood == items.end()), "Item not found.");

        auto referral_itr = referrals.emplace( _code, [&]( auto& referral ){
            referral.id	            = referrals.available_primary_key();
            referral.serialnum      = serialnum;
            referral.potentlbuyer   = potentlbuyer;
        });
    }

    private:

        /* To reward all owners, it's not practical in this framework to disburse
         * rewards to everyone on the contract's resources. Instead, one reward
         * record is created per sale and can be claimed with claimreward() by
         * users.
         */
        void rewardowners ( const uint64_t productid, const name owner, const uint64_t rewardamt ) {

            auto items_search = items.get_index<name("productid")>();
            auto items_owned = std::distance(items_search.cbegin(),items_search.cend()); 

            auto reward_itr = rewards.emplace( _code, [&]( auto& reward ){
                reward.id			= rewards.available_primary_key();
                reward.poster       = owner;
                reward.productid    = productid;
                reward.totalamt     = rewardamt;
                reward.totalrewards = items_owned;
                reward.totalclaimed  = 0;
            });
        }

        /* Again, we'll implement deposit-then-spend, but for now we handle
         * transactions in a contract table accounts.
         */
        void transact(const name from, const name to, const double quantity)
        {
            // as usual, will need more checks here, esp. once using token/accounts/deposit-then-send
            eosio_assert(quantity > 0, "cannot send 0");

            // we'll find users by names
            auto accts_search = accounts.get_index<name("owner")>();
            auto acct_itr = accts_search.find(from.value);
            // hacky solution to template mismatch ahoy
            uint64_t acct_id = acct_itr->id;
            eosio_assert(acct_itr != accts_search.end(), "unknown sender");

            auto to_acct_itr = accts_search.find(to.value);
            // hacky solution, reprise
            uint64_t to_acct_id = to_acct_itr->id;
            eosio_assert(to_acct_itr != accts_search.end(), "unknown receipient");

            auto send_itr = accounts.find( acct_id );
            auto receive_itr = accounts.find( to_acct_id );

            accounts.modify(send_itr, _self, [&](auto &account) {
                eosio_assert(account.token_balance >= quantity, "failed: insufficient balance");
                account.token_balance -= quantity;
            });

            accounts.modify(receive_itr, _self, [&](auto &account) {
                account.token_balance += quantity;
            });
        }

        // TABLES

        struct [[eosio::table, eosio::contract("promoteit")]] item {
            uint64_t    serialnum = 0;
            name        owner;
            uint64_t    currentprice; // currently, 0 means "not for sale"; items can be gifted but not posted for $0
            uint64_t    productid;
            uint64_t primary_key()const { return serialnum; }
            uint64_t by_secondary()const { return productid; }

            EOSLIB_SERIALIZE( item, (serialnum)(owner)(currentprice)(productid) )
        };

        typedef eosio::multi_index<name("item"), item, eosio::indexed_by<name("productid"), eosio::const_mem_fun<item, uint64_t, &item::by_secondary>>> item_index;
		

        struct [[eosio::table, eosio::contract("promoteit")]] product {
            uint64_t    productid = 0;
            string      description;
            uint64_t    unissuedct;
            name        creator;
            uint64_t primary_key()const { return productid; }

            EOSLIB_SERIALIZE( product, (productid)(description)(unissuedct)(creator) )
        };

        typedef eosio::multi_index< name("product"), product > product_index;

        struct [[eosio::table, eosio::contract("promoteit")]] reward {
            uint64_t    id = 0;
            name        poster;
            uint64_t    productid;
            uint64_t    totalamt;
            uint64_t    totalrewards;
            uint64_t    totalclaimed;
            uint64_t    primary_key()const { return id; }

            EOSLIB_SERIALIZE( reward, (id)(poster)(productid)(totalamt)(totalrewards)(totalclaimed) )
        };

        typedef eosio::multi_index< name("reward"), reward > reward_index;

        struct [[eosio::table, eosio::contract("promoteit")]] claim {
            uint64_t    id = 0;
            name        claimer;
            uint64_t    rewardid;
            uint128_t   easyfindr; // hack to make it easy to search "has claimer x claimed reward y?"
            uint64_t primary_key()const { return id; }
            uint64_t by_secondary()const { return easyfindr; }


            EOSLIB_SERIALIZE( claim, (id)(claimer)(rewardid)(easyfindr) )
        };

        typedef eosio::multi_index<name("claim"), claim, eosio::indexed_by<name("easyfindr"), eosio::const_mem_fun<claim, uint64_t, &claim::by_secondary>>> claim_index;

        struct [[eosio::table, eosio::contract("promoteit")]] account {
            uint64_t    id = 0;
            name        owner;        
            double      token_balance;        
        
            uint64_t primary_key() const { return id; }
            uint64_t by_secondary()const { return owner.value; }

            EOSLIB_SERIALIZE(account, (id)(owner)(token_balance))
        };

        typedef eosio::multi_index<name("account"), account, eosio::indexed_by<name("owner"), eosio::const_mem_fun<account, uint64_t, &account::by_secondary>>> account_index;

        /** The referrals table here is for referitem() above,
         *  still unused (but for an optional alternate model later).
         */
        struct [[eosio::table, eosio::contract("promoteit")]] referral {
            uint64_t    id = 0;
            uint64_t    serialnum;
            name        potentlbuyer;
            uint64_t primary_key()const { return id; }
        
            EOSLIB_SERIALIZE( referral, (id)(serialnum)(potentlbuyer) )
        };

        typedef eosio::multi_index< name("referral"), referral > referral_index;

        item_index	    items;
        product_index	products;
        claim_index	    claims;
		account_index	accounts;
        reward_index	rewards;
        referral_index  referrals;

};

EOSIO_DISPATCH( promoteit, (createacct)(cleartables)(issueitem)(createitem)(postitem)(buyitem)(claimreward)(findrewards) )
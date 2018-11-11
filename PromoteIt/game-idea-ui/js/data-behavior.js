//eosjs calls and setup

const EOS_CONFIG = {
  //if you want to call a specific contract
  contractName: "promoteit", // Contract name
  contractSender: "james", // User executing the contract (should be paired with private key)

  clientConfig: {
    //NOTE: NEVER put your private key in source, for demo purposes ONLY
    keyProvider: ['5JZSuvSL928LqUmmuzkWowBqVXmMHPDapNT6HFhH26Xw3SEaTHF'], // Your private key
    httpEndpoint: 'http://0.0.0.0:7777' // EOS http endpoint
  }
}

let eos = Eos(EOS_CONFIG.clientConfig)
console.log(eos.getInfo({}));

//method for push and pull requests to the chain from elements
function eosRequest(method, elementID) {
  console.log(method, elementID);
  switch (method) {
    case 'pull':
      switch (elementID) {
        case 'hat-hotspot':
          break;

        case 'claim-notifications':
          break;

      }
      break;

    case 'push':
      switch (elementID) {
        case 'want-button':
          $('#purchase-response').hide();
          $('#want-response').show();
          break;

        case 'purchase-button':
        eos.contract('promoteit')
          .then((contract) => {
            console.log("CONTRACT INIT");
            contract.buyitem( {
              "serialnum": "0",
              "buyername": "james"
              }, {authorization: EOS_CONFIG.contractSender} ) 
              .then((res) => {
                console.log("POST CALL");
                console.log(res);
                $('#purchase-response').show();
                $('#want-response').hide();
            }).catch((err) => { console.log(err) })
          }).catch((err) => { console.log(err) })
          break;

        case 'add-item-button':
          $('#add-item-button').hide();
          $('#add-item-response').show();
          setTimeout(function() {$('#claim-notifications').show()}, 3000);
          break;

        case 'receive-reward-button':
          $('#receive-reward-button').hide();
          $('#receive-reward-response').show();
          $('#claim-notifications').hide();
          $('#token-balance-amount').html('3.45');
          break;
      }
      break;
  }
}

//on ready
$(function() {
  //attach element _events
  $('[data-request-method]').click(function() {
    var method = $(this).attr('data-request-method');
    var id = $(this).attr('id');
    eosRequest(method, id);
  })
})

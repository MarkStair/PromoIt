//File for UI behavior/interactions

function openMenu(e, menuID) {
  e.stopPropagation();
  $( "#bottom-menu" ).animate({height: "0px"}, 500);
  $('#' + menuID + '-menu').animate({right: "0px"}, 500);
  $('body').attr('data-side-menu-state', 'open');
  $('.side-menu').removeAttr('data-menu-state');
  $('#' + menuID + '-menu').attr('data-menu-state', 'active');
}


$(function() {


  //### UI element bindings for display behavior
  //tracks location of y axis of mouse, triggers menu animation if near bottom
  $(document).mousemove(function(e) {
    mouseY = e.clientY;
    docHeight = $(document).height();
    if ($('body').attr('data-side-menu-state') !== 'open') {
      if (mouseY > ((docHeight)-140) && typeof triggered == 'undefined') {
        $('#bottom-menu-trigger').hide();
        $("#bottom-menu" ).animate({height: "150px"}, 500);
        triggered = true;
      } else if (mouseY < ((docHeight)-140) && typeof triggered !== 'undefined') {
        triggered = undefined;
        $('#bottom-menu-trigger').show();
        $( "#bottom-menu" ).animate({height: "0px"}, 500);
      }
    }
  });


  $('#hat-hotspot').mouseover(function() {
    if ($('body').attr('data-side-menu-state') !== 'open') {
      $('#game-shot').css('background-image', 'url("img/screenshot-hover.jpg")');
    }
  })

  $('#hat-hotspot').mouseout(function() {
    if ($('body').attr('data-side-menu-state') !== 'open') {
      $('#game-shot').css('background-image', 'url("img/screenshot.jpg")');
    }
  })


  $('#hat-hotspot').click(function(e) {
    e.stopPropagation();
    $('.side-menu').animate({right: "-350px"}, 500);
    $('#game-shot').css('background-image', 'url("img/screenshot-active.jpg")');
    openMenu(e, 'item-details');
  });


  $('.side-menu').click(function(e) {
    e.stopPropagation();
  })


  $('.menu-item:not(#want-list)').click(function(e) {
    openMenu(e, $(this).attr('id'));
  })


  $('#add-item-form').on('submit', function(e){
    e.preventDefault();
  })



  $(document).click(function() {
    $('#game-shot').css('background-image', 'url("img/screenshot.jpg")');
    $('[data-menu-state=active]').animate({right: "-350px"}, 500);
    $('body').attr('data-side-menu-state', 'closed');
    $('.action-response').hide();
    $('#add-item-button').show();
    $('#receive-reward-button').show();
  })


  //https://stackoverflow.com/questions/7790561/how-can-i-make-the-html5-number-field-display-trailing-zeroes
  //make number input display trailing zero
  $('#add-item-cost').on('click', function(){
    $(this).val(parseFloat($(this).val()).toFixed(2));
    //if 0, display "Free"
    if ($(this).val() == "0.00") {
      $(this).attr('type', 'text');
      $(this).val('Free');
    } else {
      $(this).attr('type', 'number');
    }
  });
  //### END UI element bindings for display behavior

})

(function ($) {

Drupal.behaviors.facetapi = {
  attach: function(context, settings) {
    // Ensures ALL soft limit select boxes are updated.
    // @see http://drupal.org/node/735528 for why this is needed.
    $('select[name="soft_limit"]').change(function() {
      $('select[name="soft_limit"]').val($(this).val());
    });
  }
}

})(jQuery);

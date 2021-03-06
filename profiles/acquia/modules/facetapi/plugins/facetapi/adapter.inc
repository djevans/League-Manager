<?php

/**
 * @file
 * Adapter plugin and adapter related calsses.
 */

/**
 * Abstract class extended by search backends that retrieves facet information
 * from the database.
 */
abstract class FacetapiAdapter {

  /**
   * Searcher information retrieved by the hook.
   *
   * @var array
   */
  protected $info = array();

  /**
   * The search keys passed by the user.
   *
   * @var string
   */
  protected $keys;

  /**
   * An array of query type plugin definitions keyed by type.
   *
   * @var array
   */
  protected $queryTypes = array();

  /**
   * An array of FacetapiFacet objects.
   *
   * @var array
   */
  protected $facets = array();

  /**
   * An array of FacetapiFacetProcessor objects.
   *
   * @var array
   */
  protected $processors = array();

  /**
   * An array of executed query type plugins keyed by field name.
   *
   * @var array
   */
  protected $facetQueries = array();

  /**
   * An array of active filters.
   *
   * @var array
   */
  protected $activeItems;

  /**
   * An array of parameters, such as from $_GET.
   *
   * @var array
   */
  protected $params;

  /**
   * A boolean flagging whether the facets have been processed.
   *
   * @var boolean
   */
  protected $processed = FALSE;

  /**
   * Define the array key in the params that is used for filters.
   */
  const FILTER_KEY = 'f';

  /**
   * Constructor, sets searcher and type of content being indexed.
   *
   * @param array $searcher_info
   *   The searcher definition.
   */
  public function __construct(array $searcher_info) {
    $this->info = $searcher_info;

    // Registers the query type plugins classes associated with this adapter.
    foreach (ctools_get_plugins('facetapi', 'query_types') as $plugin) {
      if ($searcher_info['adapter'] == $plugin['handler']['adapter']) {
        $type = call_user_func(array($plugin['handler']['class'], 'getType'));
        $this->queryTypes[$type] = $plugin['handler']['class'];
      }
    }
  }

  /**
   * Returns a boolean flagging whether $this->info['searcher'] executed a search.
   *
   * @return
   *   A boolean flagging whether $this->info['searcher'] executed a search.
   *
   * @todo Generic search API should provide consistent functionality.
   */
  abstract public function searchExecuted();

  /**
   * Returns a boolean flagging whether facets in a realm shoud be displayed.
   *
   * Useful, for example, for suppressing sidebar blocks in some cases.
   *
   * @return
   *   A boolean flagging whether to display a given realm.
   *
   * @todo Generic search API should provide consistent functionality.
   */
  abstract public function suppressOutput($realm_name);

  /**
   * Processes a raw arrray of active filters.
   *
   * @param array $params
   *   An array of keyed params, such as $_GET.
   *
   * @return FacetapiAdapter
   *   An instance of this class.
   */
  public function setParams(array $params = array()) {
    // Need to rebuild facets if we use different params.
    $this->facets = array();

    // We never want to use 'q' or 'page' params.
    $this->params = $params;
    unset($this->params['q'], $this->params['page']);
    if (!isset($this->params[self::FILTER_KEY])) {
      $this->params[self::FILTER_KEY] = array();
    }

    // Processes active items, returns this class.
    $this->processActiveItems();
    return $this;
  }

  /**
   * Processes active items.
   */
  public function processActiveItems() {
    $this->activeItems = array('facet' => array(), 'filter' => array());

    // Groups enabled facets by facet alias.
    $enabled_aliases = array();
    foreach($this->getEnabledFacets() as $facet) {
      $enabled_aliases[$facet['field alias']][] = $facet['name'];
      $this->activeItems['facet'][$facet['name']] = array();
    }

    // Extracts valid filters from query string.
    foreach ($this->params[self::FILTER_KEY] as $pos => $filter) {
      // Bails if an object or array.
      if (!is_scalar($filter)) {
        continue;
      }

      // Performs basic parsing of the filter.
      $parts = explode(':', $filter, 2);
      if (isset($parts[1]) && isset($enabled_aliases[$parts[0]])) {

        // Stores the base item.
        $item = array(
          'field alias' => $parts[0],
          'value' => $parts[1],
          'pos' => $pos,
        );

        // Stores active items in the global active item array.
        $this->activeItems['filter'][$filter] = $item;
        $this->activeItems['filter'][$filter]['facets'] = array();

        // Stores active items per facet.
        foreach ($enabled_aliases[$parts[0]] as $facet_name) {
          $this->activeItems['filter'][$filter]['facets'][] = $facet_name;
          $this->activeItems['facet'][$facet_name][$parts[1]] = $item;
        }
      }
    }
  }

  /**
   * Returns the set parameters.
   *
   * @return array
   *   The set parameters.
   */
  public function getParams() {
    return $this->params;
  }

  /**
   * Returns all active filters.
   *
   * @return array
   *   An array of active filters.
   */
  public function getAllActiveItems() {
    return $this->activeItems['filter'];
  }

  /**
   * Returns a facet's active items.
   *
   * @param array|string $facet
   *   The facet definition or facet name,
   *
   * @return array
   *   The facet's active items.
   */
  public function getActiveItems(array $facet) {
    return $this->activeItems['facet'][$facet['name']];
  }

  /**
   * Tests whether a facet item is active by passing it's value.
   *
   * @param string $facet_name
   *   The facet name.
   * @param string $value
   *   The facet item's value.
   *
   * @return
   *   Returns 1 if the item is active, 0 if it is inactive.
   */
  public function itemActive($facet_name, $value) {
    return (int) isset($this->activeItems['facet'][$facet_name][$value]);
  }

  /**
   * Returns the id of the adapter plugin.
   *
   * @return string
   *   The machine readable if of the adapter plugin.
   */
  public function getId() {
    return $this->info['adapter'];
  }

  /**
   * Returns the machine readable name of the searcher.
   *
   * @return string
   *   The machine readable name of the searcher.
   */
  public function getSearcher() {
    return $this->info['name'];
  }

  /**
   * Returns the type of content indexed by $this->info['searcher'].
   *
   * @return
   *   The type of content indexed by $this->info['searcher'].
   */
  public function getType() {
    return $this->info['type'];
  }

  /**
   * Returns the path to the admin settings for a given realm.
   *
   * @param $realm_name
   *   The name of the realm.
   *
   * @return
   *   The path to the admin settings.
   */
  public function getPath($realm_name) {
    return $this->info['path'] . '/facets/' . $realm_name;
  }

  /**
   * Sets the search keys.
   *
   * @param string $keys
   *   The search keys entered by the user.
   *
   * @return FacetapiAdapter
   *   An instance of this class.
   */
  public function setSearchKeys($keys) {
    $this->keys = $keys;
    return $this;
  }

  /**
   * Gets the search keys.
   *
   * @return string
   *   The search keys entered by the user.
   */
  public function getSearchKeys() {
    return $this->keys;
  }

  /**
   * Returns the number of results returned by the search query.
   *
   * @return int
   *   An integer containing the number of results.
   */
  public function getResultCount() {
    return;
  }

  /**
   * Allows for backend specific overrides to the settings form.
   */
  public function settingsForm(&$form, &$form_state) {
    // Nothing to do...
  }

  /**
   * Provides default values for the backend specific settings.
   *
   * @return array
   *   The defaults keyed by setting name to value.
   */
  public function getDefaultSettings() {
    return array();
  }

  /**
   * Returns TRUE if the back-end supports "missing".
   *
   * @return bool
   *   TRUE or FALSE.
   */
  public function supportsFacetMissing() {
    return $this->info['supports facet missing'];
  }

  /**
   * Adds facet query type plugins to the queue and invokes the execute() hook
   * to allow for the backend to add filters to its native query object.
   *
   * @param mixed $query
   *   The backend's native object.
   */
  function addActiveFilters($query) {
    $this->initActiveFilters($query);
    foreach ($this->getEnabledFacets() as $facet) {
      $settings = $this->getFacet($facet)->getSettings();

      // Invoke the dependency plugins.
      $display = TRUE;
      foreach ($facet['dependency plugins'] as $id) {
        $class = ctools_plugin_load_class('facetapi', 'dependencies', $id, 'handler');
        $plugin = new $class($id, $this, $facet, $settings, $this->activeItems['facet']);
        if (NULL !== ($return = $plugin->execute())) {
          $display = $return;
        }
      }

      // Add query type plugin if dependencies were met, otherwise remove the
      // facet's active items so they don't display in the current search block
      // or appear as active in the breadcrumb trail.
      if ($display) {
        $this->addFacetQuery($facet, $query);
      }
      else {
        foreach ($this->activeItems['facet'][$facet['name']] as $item) {
          $filter = $item['field alias'] . ':' . $item['value'];
          unset($this->activeItems['filter'][$filter]);
        }
        $this->activeItems['facet'][$facet['name']] = array();
      }
    }
  }

  /**
   * Allows the backend to initialize its query object before adding the facet
   * filters.
   *
   * @param mixed $query
   *   The backend's native object.
   */
  public function initActiveFilters($query) {
    // Nothing to do ...
  }

  /**
   * Returns the enabled facets associated with the instance of the adapter.
   *
   * @param string $realm_name
   *   The machine readable name of the realm, pass NULL to get the enabled
   *   facets in all realms.
   *
   * @return array
   *   An array of enabled facets.
   */
  public function getEnabledFacets($realm_name = NULL) {
    return facetapi_get_enabled_facets($this->info['name'], $realm_name);
  }

  /**
   * Returns a FacetapiFacet instance for the facet being rendered.
   *
   * @param array $facet
   *   The facet definition.
   *
   * @return FacetapiFacet
   *   The facet rendering object object.
   */
  public function getFacet(array $facet) {
    if (!isset($this->facets[$facet['name']])) {
      $this->facets[$facet['name']] = new FacetapiFacet($this, $facet);
    }
    return $this->facets[$facet['name']];
  }

  /**
   * Adds a query type plugin to the queue.
   *
   * Parses additional information from the active items, invokes the plugin's
   * execute() method to add any filters to the query object.
   *
   * @param array $facet
   *   The facet definition.
   * @param mixed $query
   *   A mixed value containing the query in the backend's native API.
   */
  public function addFacetQuery(array $facet, $query) {
    if (isset($this->queryTypes[$facet['query type']])) {

      // Instantiates query type plugin.
      $plugin = new $this->queryTypes[$facet['query type']]($this, $facet);
      $this->facetQueries[$facet['name']] = $plugin;

      // Parses additional info from the passed value.
      foreach ($this->activeItems['facet'][$facet['name']] as $value => &$item) {
        $item += $plugin->extract($item);
      }
      // Adds filters to backend.
      $plugin->execute($query);
    }
  }

  /**
   * Returns a registered facet query
   *
   * @param array|string $facet
   *   The facet definition or facet name.
   *
   * @return FacetapiQueryTypeInterface
   *   The instantiated query type plugin.
   */
  public function getFacetQuery($facet) {
    $facet_name = (is_array($facet)) ? $facet['name'] : $facet;
    if (isset($this->facetQueries[$facet_name])) {
      return $this->facetQueries[$facet_name];
    }
  }

  /**
   * Returns the human readable value associated with a facet's raw value.
   *
   * @param string $facet_name
   *   The machine readable name of the facet.
   * @param string $value
   *   The raw value passed through the query string.
   *
   * @return string
   *   The mapped value.
   */
  public function getMappedValue($facet_name, $value) {
    if (isset($this->processors[$facet_name])) {
      return $this->processors[$facet_name]->getMappedValue($value);
    }
    else {
      return array('#markup' => $value);
    }
  }

  /**
   * Sets the breadcrumb trail on searches.
   */
  public function setBreadcrumb() {
    $breadcrumb = drupal_get_breadcrumb();
    $enabled_facets = $this->getEnabledFacets();

    $keys = $this->getSearchKeys();
    $active_items = $this->getAllActiveItems();
    // Make sure the search page itself is in the breadcrumb if we are going to
    // add more crumbs. Wow this breadcrumb code sucks.
    $item = menu_get_item();
    $last_load_func = is_array($item['load_functions']) ? end($item['load_functions']) : NULL;
    if ((!$keys && $active_items) || ($keys && $last_load_func != 'menu_tail_load')) {
      $last = end($breadcrumb);
      $this_page = l($item['title'], $item['href'], $item['localized_options']);
      if ($last != $this_page) {
        $breadcrumb[] = $this_page;
      }
    }
    // Adds the current search to the query.
    // @todo Need a getBaseQuery() method or something.
    if ($keys) {
      // The last item should be text, not a link.
      $breadcrumb[] = $active_items ? l($keys, $_GET['q']) : check_plain($keys);
    }

    // Initializes base breadcrumb query.
    $query = $this->params;
    unset($query[self::FILTER_KEY]);

    // Adds filters to the breadcrumb trail.
    $last = end($active_items);
    foreach ($active_items as $item) {
      $query[self::FILTER_KEY][] = $item['field alias'] . ':' . $item['value'];

      // Replaces with the mapped value.
      $value = $this->getMappedValue($item['facets'][0], $item['value']);

      // The last item should be text, not a link.
      if ($last == $item) {
        $breadcrumb[] = !empty($value['#html']) ? $value['#markup'] : check_plain($value['#markup']);
      }
      else {
        // Appends the filter to the breadcrumb trail.
        $breadcrumb[] = l($value['#markup'], $_GET['q'], array('query' => $query, 'html' => !empty($value['#html'])));
      }
    }

    // Sets the breadcrumb trail with searck keys and filters.
    drupal_set_breadcrumb($breadcrumb);
  }

  /**
   * Builds the content for the current search block.
   *
   * @return array
   *   The block's render array.
   */
  public function buildCurrentSearch() {
    $items = array();

    // Makes sure facet builds are initialized.
    $this->processFacets();

    // Adds search keys.
    // @todo Need a getBaseQuery() method or something.
    if ($keys = $this->getSearchKeys()) {
      $items[] = check_plain($keys);
    }

    // Adds active facets to the current search block.
    foreach ($this->getAllActiveItems() as $item) {

      // Gets all children so they can be deactivated as well.
      $values = array();
      foreach ($item['facets'] as $facet_name) {
        $values = array_merge($values, $this->processors[$facet_name]->getActiveChildren($item['value']));
      }
      // Handle the case of a URL value that matches no actual
      // facet values. Otherwise, it can't be unclicked.
      if (!in_array($item['value'], $values)) {
        $values[] = $item['value'];
      }

      // Builds variables for active link theme.
      $mapped = $this->getMappedValue($item['facets'][0], $item['value']);

      $variables = array(
        'text' => $mapped['#markup'],
        'path' => $_GET['q'],
        'options' => array(
          'attributes' => array('class' => array()),
          'html' => !empty($mapped['#html']),
          'query' => $this->processors[$item['facets'][0]]->getQueryString($values, 1),
        ),
      );

      // Renders the active link.
      $items[] = theme('facetapi_link_active', $variables);
    }

    // If there are items, return the render array.
    if ($items) {
      $content = array(
        '#title' => t('Current search'),
        'block' => array(
          '#theme' => 'item_list',
          '#items' => $items,
        ),
      );

      // Set message as title if list if result count was returned.
      if (NULL !== ($count = $this->getResultCount())) {
        $content['block']['#title'] = format_plural($count, 'Search found 1 item', 'Search found @count items');
      }

      // Returns the render array for the block.
      return $content;
    }
  }

  /**
   * Initializes facet builds, adds breadcrumb trail.
   */
  protected function processFacets() {
    if (!$this->processed) {
      $this->processed = TRUE;

      // Initializes each facet's render array.
      foreach ($this->getEnabledFacets() as $facet) {
        $this->processors[$facet['name']] = new FacetapiFacetProcessor(
          $this->getFacet($facet), self::FILTER_KEY
        );
        $this->processors[$facet['name']]->process();
      }

      // Sets the breadcrumb trail if a search was executed.
      if ($this->searchExecuted()) {
        $this->setBreadcrumb();
      }
    }
  }

  /**
   * Builds the render array for facets in a realm.
   *
   * @param string $realm_name
   *   The machine readable name of the realm.
   *
   * @return array
   *   The render array.
   */
  public function buildRealm($realm_name) {
    // Bails if realm isn't valid.
    // @todo Call watchdog()?
    if (!$realm = facetapi_realm_load($realm_name)) {
      return array();
    }

    // Makes sure facet builds are initialized.
    $this->processFacets();

    // Adds JavaScript, initializes render array.
    drupal_add_js(drupal_get_path('module', 'facetapi') . '/facetapi.js');
    $build = array(
      '#adapter' => $this,
      '#realm' => $realm,
    );

    // Builds each facet in the realm, merges into realm's render array.
    foreach ($this->getEnabledFacets($realm['name']) as $facet) {

      // Gets the initialized build.
      $field_alias = $facet['field alias'];
      $processor = $this->processors[$facet['name']];
      $facet_build = $this->getFacet($facet)->build($realm, $processor);

      // Tries to be smart when merging the render arrays. Crazy things happen
      // when merging facets with the same field alias such as taxonomy terms in
      // the fieldset realm. We want to merge only the values.
      foreach (element_children($facet_build) as $child) {
        // Bails if there is nothing to render.
        if (!element_children($facet_build[$child])) {
          continue;
        }
        // Attempts to merge gracefully.
        if (!isset($build[$child])) {
          $build = array_merge_recursive($build, $facet_build);
        }
        else {
          if (isset($build[$child][$field_alias]) && isset($facet_build[$child][$field_alias])) {
            $build[$child][$field_alias] = array_merge_recursive(
              $build[$child][$field_alias],
              $facet_build[$child][$field_alias]
            );
          }
          elseif (isset($build[$child]['#options']) && isset($facet_build[$child]['#options'])) {
            $build[$child]['#options'] = array_merge_recursive(
              $build[$child]['#options'],
              $facet_build[$child]['#options']
            );
          }
          else {
            $build = array_merge_recursive($build, $facet_build);
          }
        }
      }
    }

    // Allows modules to alter the render array before returning.
    drupal_alter('facetapi_facets', $build, $this, $realm);
    return $build;
  }
}

/**
 * Stores facet data, provides methods that build the facet's render array.
 */
class FacetapiFacet implements ArrayAccess {

  /**
   * The FacetapiAdapter object.
   *
   * @var FacetapiAdapter
   */
  protected $adapter;

  /**
   * The facet definition.
   *
   * @var array
   */
  protected $facet;

  /**
   * The build array for the facet items.
   *
   * @var array
   */
  protected $build = array();

  /**
   * The initialized query string for all facets.
   *
   * @var array
   */
  protected $params;

  /**
   * Constructor, sets adapter and facet definition.
   *
   * @param $adapter
   *   A FacetapiAdapter object.
   * @param $facet
   *   An array containing the facet definition.
   */
  public function __construct(FacetapiAdapter $adapter, array $facet) {
    $this->adapter = $adapter;
    $this->facet = $facet;
  }

  /**
   * Whether a offset exists
   *
   * @param mixed offset
   *   An offset to check for.
   *
   * @return boolean
   */
  public function offsetExists($offset) {
    return isset($this->facet[$offset]);
  }

  /**
   * Returns the value at specified offset.
   *
   * @param mixed offset
   *   The offset to retrieve.
   *
   * @return mixed
   */
  public function offsetGet($offset) {
    return isset($this->facet[$offset]) ? $this->facet[$offset] : NULL;
  }

  /**
   * Assigns a value to the specified offset.
   *
   * @param mixed offset
   *   The offset to assign the value to.
   * @param mixed value
   *   The value to set.
   */
  public function offsetSet($offset, $value) {
    if (NULL === $offset) {
      $this->facet[] = $value;
    }
    else {
      $this->facet[$offset] = $value;
    }
  }

  /**
   * Unsets an offset.
   *
   * @param mixed offset
   *   The offset to unset.
   */
  public function offsetUnset($offset) {
    unset($this->facet[$offset]);
  }

  /**
   * Returns the adapter object.
   *
   * @return FacetapiAdapter
   *   The adapter object.
   */
  public function getAdapter() {
    return $this->adapter;
  }

  /**
   * Returns the facet definition.
   *
   * @return array
   *   An array containing the facet definition.
   */
  public function getFacet() {
    return $this->facet;
  }

  /**
   * Returns the facet definition.
   *
   * @return array
   *   An array containing the facet definition.
   */
  public function getBuild() {
    return $this->build;
  }

  /**
   * Gets facet setting for the passed realm.
   *
   * @param string|array $realm
   *   The machine readable name of the realm or realm definition. Pass null to
   *   get global settings.
   *
   * @return
   *   An object containing the settings.
   */
  public function getSettings($realm = NULL) {
    // Normalizes the realm name.
    if (!is_array($realm)) {
      $realm_name = ($realm) ? $realm : '';
      $realm = ($realm) ? facetapi_realm_load($realm_name) : array();
    }
    else {
      $realm_name = $realm['name'];
    }

    // Loads the settings via the CTools API.
    ctools_include('export');
    $name = $this->adapter->getSearcher() . ':' . $realm_name . ':' . $this->facet['name'];
    if (!$settings = ctools_export_crud_load('facetapi', $name)) {

      // Initializes settings object.
      $settings = ctools_export_crud_new('facetapi');
      $settings->name = $name;
      $settings->searcher = $this->adapter->getSearcher();
      $settings->realm = $realm_name;
      $settings->facet = $this->facet['name'];
      $settings->enabled = 0;

      if ($realm) {

        if (!empty($this->facet['default widget'])) {
          $widget = $this->facet['default widget'];
        }
        else {
          $widget = $realm['default widget'];
        }
        // Initializes realm-specific settings.
        $settings->settings = array(
          'weight' => 0,
          'widget' => $widget,
          'active_sorts' => array(),
          'sort_weight' => array(),
          'sort_order' => array(),
          'empty_behavior' => 'none',
        );

        // Apply default sort settings.
        $weight = -50;
        foreach ($this->facet['default sorts'] as $sort => $default) {
          $settings->settings['active_sorts'][$default[0]] = $default[0];
          $settings->settings['sort_weight'][$default[0]] = $weight++;
          $settings->settings['sort_order'][$default[0]] = $default[1];
        }

        // Loads default widget plugin, adds settings.
        $id = $settings->settings['widget'];
        $class = ctools_plugin_load_class('facetapi', 'widgets', $id, 'handler');
        // Fallback in case we have an invalid default widgt.
        if (!$class) {
          $id = $settings->settings['widget'] = $realm['default widget'];
          $class = ctools_plugin_load_class('facetapi', 'widgets', $id, 'handler');
        }
        $plugin = new $class($id, $realm, $this, $settings);
        $settings->settings += $plugin->getDefaultSettings();

      }
      else {
        // Initializes settings.
        $settings->settings = array(
          'operator' => FACETAPI_OPERATOR_AND,
          'hard_limit' => 50,
          'dependencies' => array(),
          'facet_missing' => 0,
        );

        // Loads the default settings from the adapter.
        $settings->settings += $this->adapter->getDefaultSettings();

        // Loads default settings from dependency plugins.
        foreach ($this->facet['dependency plugins'] as $id) {
          $class = ctools_plugin_load_class('facetapi', 'dependencies', $id, 'handler');
          $plugin = new $class($id, $this->adapter, $this->facet, $settings, $this->adapter->getAllActiveItems());
          $settings->settings['dependencies'] = array();
          $settings->settings['dependencies'] += $plugin->getDefaultSettings();
        }

      }

      // @todo Explore whether we should save settings for performance.
    }

    // Ensures settings added in later versions of the module are initialized to
    // prevent undefuned index errors after upgrading.
    // @todo Remove in later versions.
    if ($realm) {
      $settings->settings += array('empty_behavior' => 'none');
    }
    else {
      $settings->settings += array('facet_missing' => 0);
    }

    return $settings;
  }

  /**
   * Returns the facet's render array.
   *
   * @param array $realm
   *   An array containing the realm definition.
   * @param FacetapiFacetProcessor $processor
   *   The processor object.
   *
   * @return
   *   The facet's build array.
   */
  public function build(array $realm, FacetapiFacetProcessor $processor) {
    $settings = $this->getSettings($realm);

    // Gets the base render array from the facet processor.
    $this->build = $processor->getBuild();

    // Instantiates the widget plugin and initializes.
    $widget_name = $settings->settings['widget'];
    $class = ctools_plugin_load_class('facetapi', 'widgets', $widget_name, 'handler');
    $widget_plugin = new $class($widget_name, $realm, $this, $settings);
    $widget_plugin->init();

    // Executes widget plugin if not empty, otherwise executes the empty
    // behavior plugin.
    if ($this->build) {
      $widget_plugin->execute();
      $build = $widget_plugin->getBuild();
    }
    else {
      // Instantiates empty behavior plugin.
      $id = $settings->settings['empty_behavior'];
      $class = ctools_plugin_load_class('facetapi', 'empty_behaviors', $id, 'handler');
      $empty_plugin = new $class($settings);
      // Executes empty behavior plugin.
      $build = $widget_plugin->getBuild();
      $build[$this['field alias']] = $empty_plugin->execute();
    }

    // If the element is empty, unset it.
    if (!$build[$this['field alias']]) {
      unset($build[$this['field alias']]);
    }

    // Adds JavaScript settings in a way that merges with others already set.
    $merge_settings['facetapi']['facets'][] = $widget_plugin->getJavaScriptSettings();
    drupal_add_js($merge_settings, 'setting');

    // Returns array keyed by the FacetapiWidget::$key property.
    return array($widget_plugin->getKey() => $build);
  }
}

/**
 * Processes facets, initializes the build.
 */
class FacetapiFacetProcessor {

  /**
   * An array of mapped values keyed by their raw value.
   *
   * @var $map
   */
  protected $map = array();

  /**
   * The facet being processed.
   *
   * @var FacetapiFacet
   */
  protected $facet;

  /**
   * The key in $params corresponding to filters.
   *
   * @var string
   */
  protected $filterKey;

  /**
   * The facet's initialized render array.
   *
   * @var array
   */
  protected $build = array();

  /**
   * Arrays of children keyed by their active parent's value.
   *
   * @var array
   */
  protected $activeChildren = array();

  /**
   * Constructor, initializes render array.
   *
   * @param FacetapiFacet $facet
   *
   * @param $filter_key
   *   The array key in $params corresponding to filters. $params[$filter_key]
   *   must be an array.
   *
   */
  public function __construct(FacetapiFacet $facet, $filter_key) {
    $this->facet = $facet;
    $this->filterKey = $filter_key;
  }

  /**
   * Processes the facet items.
   */
  public function process() {
    $this->build = array();

    // Only initializes facet if a query type plugin is registered for it.
    // NOTE: We don't use the chaining pattern so the methods can be tested.
    if ($this->facet->getAdapter()->getFacetQuery($this->facet->getFacet())) {
      if ($this->build = $this->initializeBuild($this->build)) {
        $this->build = $this->mapValues($this->build);
        $this->build = $this->processHierarchy($this->build);
        $this->processQueryStrings($this->build);
      }
    }
  }

  /**
   * Gets an active item's children.
   *
   * @param string $value
   *   The value of the active item.
   *
   * @return array
   *   The active item's childen.
   */
  public function getActiveChildren($value) {
    return (isset($this->activeChildren[$value])) ? $this->activeChildren[$value] : array();
  }

  /**
   * Gets the initialized render array.
   */
  public function getBuild() {
    return $this->build;
  }

  /**
   * Returns the human readable value associated with a raw value.
   *
   * @param string $value
   *   The raw value passed through the query string.
   *
   * @return string
   *   The mapped value.
   */
  public function getMappedValue($value) {
    return (isset($this->map[$value])) ? $this->map[$value] : array('#markup' => $value);
  }

  /**
   * Initializes the facet's render array.
   *
   * @return array
   *   The initialized render array.
   */
  protected function initializeBuild() {
    $build = array();

    // Bails if there is no field attached to the facet, in other words if the
    // facet is simply rendering markup.
    if (!$this->facet['field']) {
      return $build;
    }

    // Build array defaults.
    $defaults = array(
      '#markup' => '',
      '#html' => FALSE,
      '#indexed_value' => '',
      '#count' => 0,
      '#active' => 0,
      '#item_parents' => array(),
      '#item_children' => array(),
    );

    // Builds render arrays for each item.
    $adapter = $this->facet->getAdapter();
    $build = $adapter->getFacetQuery($this->facet->getFacet())->build();
    foreach (element_children($build) as $value) {
      $item_defaults = array(
        '#markup' => $value,
        '#indexed_value' => $value,
        '#active' => $adapter->itemActive($this->facet['name'], $value),
      );
      $build[$value] = array_merge($defaults, $item_defaults, $build[$value]);
    }

    return $build;
  }

  /**
   * Maps the IDs to human readable values via the mapping callback.
   *
   * @param array $build
   *   The initialized render array.
   *
   * @return array
   *   The initialized render array with mapped values.
   */
  protected function mapValues(array $build) {
    if ($this->facet['map callback']) {
      $this->map = call_user_func($this->facet['map callback'], array_keys($build), $this->facet['map options']);
      foreach ($this->map as $key => $value) {
        // Normalize all mapped values to a two element array.
        if (!is_array($value)) {
          $this->map[$key] = array();
          $this->map[$key]['#markup'] = $value;
          $this->map[$key]['#html'] = FALSE;
        }
        if (isset($build[$key])) {
          $build[$key]['#markup'] = $this->map[$key]['#markup'];
          $build[$key]['#html'] = !empty($this->map[$key]['#html']);
        }
      }
    }
    return $build;
  }

  /**
   * Processes hierarchical relationships between the facet items.
   *
   * @param array $build
   *   The initialized render array.
   *
   * @return array
   *   The initialized render array with processed hierarchical relationships.
   */
  protected function processHierarchy(array $build) {

    // Builds the hierarchy information if the hierarchy callback is defined.
    if ($this->facet['hierarchy callback']) {
      $parents = $this->facet['hierarchy callback'](array_keys($build));
      foreach ($parents as $value => $parents) {
        foreach ($parents as $parent) {
          if (isset($build[$parent]) && isset($build[$value])) {
            // Use a reference so we see the updated data.
            $build[$parent]['#item_children'][$value] = &$build[$value];
            $build[$value]['#item_parents'][$parent] = $parent;
          }
        }
      }
    }

    // Tests whether parents have an active child.
    // @todo: Can we make this more efficient?
    do {
      $active = 0;
      foreach ($build as $value => $item) {
        if ($item['#active'] && !empty($item['#item_parents'])) {
          // @todo Can we build facets with multiple parents? Core taxonomy
          // form cannot, so we will need a check here.
          foreach ($item['#item_parents'] as $parent) {
            if (!$build[$parent]['#active']) {
              $active = $build[$parent]['#active'] = 1;
            }
          }
        }
      }
    } while ($active);

    // Strips children whose parents are inactive.
    return array_filter($build, 'facetapi_filter_inactive_parent');
  }

  /**
   * Initializes the render array's query string variables.
   *
   * @param array &$build
   *   The initialized render array.
   */
  protected function processQueryStrings(array &$build) {
    foreach ($build as $value => &$item) {
      $values = array($value);
      if ($item['#active']) {
        // If the item is active an has children, gets the paths for the
        // children. Merges child values with this facet item's value so that
        // unclicking the parent deactivates the children as well.
        if (!empty($item['#item_children'])) {
          $this->processQueryStrings($item['#item_children']);
          $values = array_merge(facetapi_get_child_values($item['#item_children']), $values);
        }
        // Stores this item's active children so we can deactivate them in the
        // current search block as well.
        $this->activeChildren[$value] = $values;
      }

      // Formats query string for facet item, sets theme function.
      $item['#query'] = $this->getQueryString($values, $item['#active']);
    }
  }

  /**
   * Helper function that returns the query string variables for a facet item.
   *
   * @param array $values
   *   An array containing the item's values being added to or removed from the
   *   query string dependent on whether or not the item is active.
   * @param int $active
   *   An integer flagging whether the item is active or not.
   *
   * @reutrn
   *   An array containing the query string variables.
   */
  public function getQueryString(array $values, $active) {
    $adapter = $this->facet->getAdapter();
    $qstring = $adapter->getParams();
    $active_items = $adapter->getActiveItems($this->facet->getFacet());

    // Appends to qstring if inactive, removes if inactive.
    foreach ($values as $value) {
      if ($active && isset($active_items[$value])) {
        unset($qstring[$this->filterKey][$active_items[$value]['pos']]);
      }
      elseif (!$active) {
        $qstring[$this->filterKey][] = $this->facet['field alias'] . ':' . $value;
      }
    }

    // Resets array keys, returns query string.
    $qstring[$this->filterKey] = array_values($qstring[$this->filterKey]);
    return array_filter($qstring);
  }
}

/**
 * Recursive function that returns an array of values for all descendants of a
 * facet item.
 *
 * @param $build
 *   A render array containing the facet item's children.
 *
 * @return
 *   An array containing the values of all descendants.
 */
function facetapi_get_child_values(array $build) {
  $values = array_keys($build);
  foreach ($build as $item) {
    if (!empty($item['#item_children'])) {
      $values = array_merge(facetapi_get_child_values($item['#item_children']), $values);
    }
  }
  return $values;
}

/**
 * Callback for array_filter() that strips out all children whose parents are
 * inactive.
 *
 * @param $build
 *   The facet item's render array.
 *
 * @return
 *   A boolean flagging whether the value should remain in the array.
 */
function facetapi_filter_inactive_parent(array $build) {
  return empty($build['#item_parents']);
}

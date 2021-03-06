<?php

/**
 * Gets access rules.
 *
 * @param $op
 *  $op - Permission requested
 * @return Boolean
 *  Boolean TRUE if the current user has the requested permission
 */
function _form_content_resource_access($op = 'view', $args = array()) {  
  switch ($op) {
    case 'view':
      return user_access('administer users');
      break;
    case 'create':
      return user_access('administer users');
      break;
  }
}


/**
 * Gets all resource definitions.
 *
 * @return array
 *  An array containing all resources.
 */
function form_content_resources() { 
  $resource = array(
    'retrieve' => array(
      'callback' => '_form_content_resources_retrieve',
        'args' => array(
          array(
            'name' => 'type',
            'optional' => TRUE,
            'source' => array('path' => 0),
            'type' => 'string',
            'description' => 'Provide name of content type to get form structure',
          ),
        ),
        'access callback' => '_form_content_resource_access',
        'access arguments' => array('view'),
        'access arguments append' => TRUE,
    ),
    'index' => array(
      'callback' => '_form_content_resources_index',
        'args' => array(
          array(
            'name' => 'entity_type',
            'optional' => TRUE,
            'type' => 'string',
            'description' => 'The zero-based index of the page to get, defaults to 0.',
            'source' => array('param' => 'entity_type'),
          ),
          array(
            'name' => 'bundle',
            'optional' => FALSE,
            'type' => 'string',
            'description' => 'The fields to get.',
            'source' => array('param' => 'bundle'),
          ),
        ),
        'access callback' => '_form_content_resource_access',
        'access arguments' => array('view'),
        'access arguments append' => TRUE,
    ),
  );
  return $resource;
}

function _form_content_resources_index($entity_type, $bundle) {

  include_once DRUPAL_ROOT . '/includes/iso.inc';
  if(!isset($entity_type))
  {
      return services_error("Entity type is requied.", 406, null);
  }
  if (!isset($bundle))
  {
    return services_error("Entity bundle is requied.", 406, null);
  }

  $fields = field_info_instances($entity_type, $bundle); 
  if(!empty($fields))
  {  
    foreach($fields as $field_id => $value)
    {
      $field_info = field_info_field($field_id);
      $cardinality = $field_info['cardinality'];
      $index = $fields[$field_id]['widget']['weight'];
      $widget_type = $fields[$field_id]['widget']['type'];
      $widget_module = $fields[$field_id]['widget']['module'];
      $module_type = $fields[$field_id]['display']['default']['module'];
      $widget_settings = $fields[$field_id]['widget']['settings'];
      $value['cardinality'] =  $cardinality;
       
      $required = $value['required'];
      
      if($module_type == 'taxonomy')
      {
         $taxonomy = taxonomy_vocabulary_machine_name_load($field_info['settings']['allowed_values'][0]['vocabulary']);
         $value['options'] = taxonomy_get_tree($taxonomy->vid);
      }
      if($module_type == 'list') {
        $allowed_values = $field_info['settings']['allowed_values']; 
        $opt = array();
        $i = 0;
        if($widget_type != "options_onoff" &&  $cardinality == 1)// || $widget_type == "options_select")
        {
          $i = 1;
          $opt[0]['key'] = "_none";
          $opt[0]['value'] = "-none-";
        }
        if(!empty($allowed_values)) {
          foreach($allowed_values as $key => $val)
          {
            $opt[$i]['key'] = $key;
            $opt[$i]['value'] = $val;
            $i++;
          }
          $value['options'] = $opt;
        }
      } 
      if($module_type == 'commerce_line_item')
      { 
        $line_item_type = commerce_line_item_type_get_name();
        foreach($line_item_type as $type => $label)
        {
           $line_type['line_item_type'] = $type;
           $line_type['line_item_type_label'] = $label;
           $value['commerce_line_item_types'][] = $line_type;
        } 
        if(isset($line_item_type['product']))
        {
          $value['commerce_line_item_type_product'] = commerce_products();
        }
      } 
      if($module_type == 'addressfield')
      { 
        $format_handler = $value['widget']['settings']['format_handlers'];
        if(is_object($format_handler))
        {
          if(empty($value['widget']['settings']['available_countries']) && $format_handler['address'])
          {
            $value['widget']['settings']['available_countries'] = _get_predefine_countries();
          }
        }
        else if(is_array($format_handler))
        {
          foreach($format_handler as $index => $format)
          {
            if(empty($value['widget']['settings']['available_countries']) && $format == 'address')
            {
              $value['widget']['settings']['available_countries'] = _get_predefine_countries();
            }
          }
        }
      }
      if($value['widget']['type'] == 'commerce_customer_profile_manager')
      {
         $value['customer_profile_entity_form'] = _form_content_resources_index('commerce_customer_profile', 'billing');
      }
      if($widget_module == 'commerce_price')
      {
        $default_currency_code = $widget_settings['currency_code'];
        if (empty($default_currenct_code) || $default_currenct_code == 'default') {
          $default_currency_code = NULL;
        } 
        else {
          $default_currency_code = $instance['widget']['settings']['currency_code'];
        }
        $default_currency = commerce_currency_load($default_currency_code);
        switch($widget_type) {
          case 'commerce_price_simple':
            $value['currency_code']['default_currency_code'] = $default_currency['code'];
            break;
          case 'commerce_price_full':
            $value['currency_code']['default_currency_code'] = $default_currency['code'];
            foreach (commerce_currencies(TRUE) as $currency_code => $currency) {
              $options[$currency_code] = t('@code - @name', array('@code' => $currency['code'], '@name' => $currency['name']));
            }
            $value['currency_code']['options'] = $options;
            break;
        }
      }
      $field_object->$field_id = $value; 
    }
  }
  return $field_object;
}

function _get_predefine_countries()
{
  foreach(_country_get_predefined_list() as $country_code => $country_name)
  {
    $countries[$country_code] = $country_code;
  }
  return $countries;
}

function commerce_products()
{
  $entity_type = 'commerce_product';
  $entity_info = entity_get_info($entity_type);
  
  // Build a query to load all accessible entities.
  $query = new EntityFieldQuery();
  $query
    ->entityCondition('entity_type', $entity_type);

  // Add the access query tag if specified for the entity type.
  if (!empty($entity_info['access arguments']['access tag'])) {
    $query->addTag($entity_info['access arguments']['access tag']);
  }

  // Execute the query and load the entities it returns if any.
  $result = $query->execute();

  
  $entities = array();
  $expand_entities = 1;
  $flatten_fields = 'true';
  $product_entities = array();
  if (!empty($result[$entity_type])) {
    $entities = entity_load($entity_type, array_keys($result[$entity_type]));
    foreach ($entities as $entity_id => &$entity) {
      // Add simplified fields to the entity object for certain field types.
      commerce_services_decorate_entity($entity_type, $entity);

      // Expand referenced entities into the full entity to the specified depth.
      if ($expand_entities > 0) {
        commerce_services_expand_entities($entity_type, $entity, $expand_entities, $flatten_fields == 'true');
      }

      // Flatten field value arrays if specified. This must be the last operation
      // performed as it breaks the standard field data model. An entity whose
      // fields have thus been flattened is no longer wrappable or writable.
      if ($flatten_fields == 'true') {
        $entity = clone($entity);
        commerce_services_flatten_fields($entity_type, $entity);
      }
      $product_entities[] = $entity;
    }
  }
  return $product_entities;
}

/**
 * Returns the form structure. 
 *
 * @param $type
 *   $type - content type name
 * @return
 *   form structure
 */
function _form_content_resources_retrieve($type) { 
  if (isset($type)) {
 
    // At first we need to get field's id. If you already know field id, you can ommit this step
    // Get all fields attached to a given node type

    $fields = field_info_instances('node', $type); 
    foreach($fields as $field_id => $value)
    {
      $field_info = field_info_field($field_id);

      $cardinality = $field_info['cardinality'];
      $index = $fields[$field_id]['widget']['weight'];
      $type = $fields[$field_id]['widget']['type'];
      $module_type = $fields[$field_id]['display']['default']['module'];
      $value['cardinality'] =  $cardinality;
      $required = $value['required'];
      $value['required'] =  ($required == 1) ? 1 : 0;
      if($module_type == 'taxonomy')
      {
         $taxonomy = taxonomy_vocabulary_machine_name_load($field_info['settings']['allowed_values'][0]['vocabulary']);
         $value['options'] = taxonomy_get_tree($taxonomy->vid);
      }
      if($module_type == 'list') {
         $allowed_values = $field_info['settings']['allowed_values']; 
         $opt = array();
         $i = 0;
         if($type != "options_onoff" &&  $cardinality == 1)// || $type == "options_select")
         {
           $i = 1;
           $opt[0]['key'] = "_none";
           $opt[0]['value'] = "-none-";
         }
         foreach($allowed_values as $key => $val)
         {
            $opt[$i]['key'] = $key;
            $opt[$i]['value'] = $val;
            $i++;
         }
         $value['options'] = $opt;
      } 
      $field_object->$field_id = $value; 
    }
    return $field_object;
  }
}

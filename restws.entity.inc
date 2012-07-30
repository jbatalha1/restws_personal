<?php

/**
 * @file
 * RESTful web services module integration for entities.
 */

/**
 * Specifies CRUD and access methods for resources.
 */
interface RestWSResourceControllerInterface {

  /**
   * Returns the property info for the given resource.
   *
   * @return array
   *   An array structured as hook_entity_property_info() is structured for an
   *   entity type.
   */
  public function propertyInfo();

  /**
   * Returns a metadata wrapper for the resource with the given id.
   *
   * @return EntityStructureWrapper
   *   Metadata wrapper of the resource.
   */
  public function wrapper($id);

  /**
   * Create a new resource.
   *
   * @param array $values
   *   Array of values for properties of the resource, keyed by property
   *   name. At least for all required properties values have to be given.
   *
   * @return int|string
   *   The id of the newly created resource.
   */
  public function create(array $values);

  /**
   * Returns an existing resource.
   *
   * @param int|string $id
   *   The id of the resource that should be returned.
   *
   * @return
   *   The internal representation of the resource.
   */
  public function read($id);

  /**
   * Update an existing resource.
   *
   * @param int|string $id
   *   The id of the resource that should be updated.
   * @param array $values
   *   An array of values for the properties to be updated, keyed by property
   *   name.
   */
  public function update($id, array $values);

  /**
   * Delete an existing resource.
   *
   * @param int|string $id
   *   The id of the resource that should be deleted.
   */
  public function delete($id);

  /**
   * Query for a list of resources.
   *
   * If a resource doesn't want to implement querying, then it should throw a
   * RestWSException with the 501 HTTP status code.
   *
   * @param array $filters
   *   A list of properties to query for, or an empty array if all resources
   *   should be returned.
   * @param array $meta_controls
   *   @see restws_meta_controls()
   *
   * @return array
   *   An array containing the ids of the matching resources.
   */
  public function query($filters = array(), $meta_controls = array());

  /**
   * Determines access for a given operation and resource.
   *
   * @param string $op
   *   Either 'create', 'view' (= read), 'update' or 'delete'.
   * @param int|string $id
   *   The id of the resource.
   *
   * @see entity_access()
   */
  public function access($op, $id);

  /**
   * Returns the name of the resource.
   */
  public function resource();
}

/**
 * Controller for entity-bases resources.
 */
class RestWSEntityResourceController implements RestWSResourceControllerInterface {

  protected $entityType, $entityInfo;

  public function __construct($name, $info) {
    $this->entityType = $name;
    $this->entityInfo = entity_get_info($name);
  }

  public function propertyInfo() {
    return entity_get_all_property_info($this->entityType);
  }

  public function wrapper($id) {
    return entity_metadata_wrapper($this->entityType, $id);
  }

  public function read($id) {
    return $this->wrapper($id)->value();
  }

  public function create(array $values) {
    try {
      $wrapper = entity_property_values_create_entity($this->entityType, $values);
    }
    catch (EntityMetadataWrapperException $e) {
      throw new RestWSException($e->getMessage(), 406);
    }

    // Make sure that bundle information is present on entities that have
    // bundles.
    $entity_info = entity_get_info($this->entityType);
    if (isset($entity_info['bundle keys'])) {
      foreach ($entity_info['bundle keys'] as $bundle_key) {
        if (!array_key_exists($bundle_key, $values)) {
          throw new RestWSException('Missing bundle: ' . $bundle_key, 406);
        }
      }
    }

    $properties = $wrapper->getPropertyInfo();
    $diff = array_diff_key($values, $properties);
    if (!empty($diff)) {
      throw new RestWSException('Unknown data properties: ' . implode(' ', array_keys($diff)) . '.', 406);
    }
    $wrapper->save();
    return $wrapper->getIdentifier();
  }

  public function update($id, array $values) {
    $wrapper = $this->wrapper($id);
    try {
      foreach ($values as $name => $value) {
        $wrapper->{$name}->set($value);
      }
    }
    catch (EntityMetadataWrapperException $e) {
      throw new RestWSException($e->getMessage(), 406);
    }
    $wrapper->save();
  }

  public function delete($id) {
    entity_delete($this->entityType, $id);
  }

  /**
   * Implements RestWSResourceControllerInterface::query().
   */
  public function query($filters = array(), $meta_controls = array()) {
    $limit = variable_get("restws_query_max_limit", 100);

    $query =  new EntityFieldQuery();
    $query->entityCondition('entity_type', $this->resource());
    $query->pager($limit);

    foreach ($filters as $filter => $value) {
      $this->propertyQueryOperation($query, 'Condition', $filter, $value);
    }

    $rest_controls = restws_meta_controls();
    foreach ($meta_controls as $control_name => $property) {
      switch ($control_name) {
        case $rest_controls['sort']:
          $direction = isset($meta_controls[$rest_controls['direction']]) ? $meta_controls[$rest_controls['direction']] : 'ASC';
          $this->propertyQueryOperation($query, 'OrderBy', $property, $direction);
          break;
      }
    }

    // Catch any errors, like wrong keywords or properties.
    try {
      $query_result = $query->execute();
    }
    catch (PDOException $exception) {
      throw new RestWSException($exception->getMessage(), 412);
    }
    $query_result = isset($query_result[$this->resource()]) ? $query_result[$this->resource()] : array();

    $result = array_keys($query_result);

    return $result;
  }

  public function access($op, $id) {
    return entity_access($op, $this->entityType, isset($id) ? $this->wrapper($id)->value() : NULL);
  }

  public function resource() {
    return $this->entityType;
  }

  /**
   * Helper function which takes care of distinguishing between fields and
   * entity properties and executes the right EntityFieldQuery function for it.
   *
   * @param EntityFieldQuery $query
   *   The EntityFieldQuery pointer which should be used.
   *
   * @param string $operation
   *   The general function name, without the words 'property' or 'field'.
   *
   * @param string $property
   *   The property or field which should be used.
   *
   * @param string $value
   *   The value for the function.
   */
  protected function propertyQueryOperation(EntityFieldQuery $query, $operation, $property, $value) {
    $properties = $this->propertyInfo();

    // If field is not set, then the filter is a property and we can extract
    // the schema field from the property array.
    if (empty($properties[$property]['field'])) {
      $column = $properties[$property]['schema field'];
      $operation = 'property' . $operation;
      $query->$operation($column, $value);
    }
    else {
      // For fields we need the field info to get the right column for the
      // query.
      $field_info = field_info_field($property);
      $column = array_keys($field_info['columns']);
      // Just pick the first field column for now.
      // @todo how can we determine a specific column?
      $column = $column[0];
      $operation = 'field' . $operation;
      $query->$operation($field_info, $column, $value);
    }
  }
}

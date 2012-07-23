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
   *   @todo Control commands for sorting or paging.
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
    return entity_get_property_info($this->entityType);
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
    $query_result = $query->execute();
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
}

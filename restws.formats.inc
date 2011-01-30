<?php
// $Id$

/**
 * @file
 * RESTful web services module formats.
 */

/**
 * Interface implemented by formatter implementations for the http client.
 */
interface RestWSFormatInterface {

  /**
   * Gets the representation of a resource.
   *
   * @param RestWSResourceControllerInterface $resourceController
   *   The controller used to retrieve the resource.
   * @param $id
   *   The id of the resource that should be returned.
   *
   * @return
   *   The representation of the resource.
   */
  public function viewResource($resourceController, $id);

  /**
   * Create a resource.
   *
   * @param RestWSResourceControllerInterface $resourceController
   *   The controller used to create the resource.
   * @param $data
   *   The representation of the resource.
   *
   * @return
   *   The id of the newly created resource.
   */
  public function createResource($resourceController, $data);

  /**
   * Update a resource.
   *
   * @param RestWSResourceControllerInterface $resourceController
   *   The controller used to update the resource.
   * @param $id
   *   The id of the resource that should be updated.
   * @param $data
   *   The representation of the resource.
   */
  public function updateResource($resourceController, $id, $data);

  /**
   * Delete a resource.
   *
   * @param RestWSResourceControllerInterface $resourceController
   *   The controller used to update the resource.
   * @param $id
   *   The id of the resource that should be deleted.
   */
  public function deleteResource($resourceController, $id);


  /**
   * Returns the mime type of this format, e.g. 'application/json' or
   * 'application/xml'.
   */
  public function mimeType();
}

/**
 * A base for all simple formats that are just serializing/unserializing an array of property values.
 */
abstract class RestWSBaseFormat implements RestWSFormatInterface {

  protected $formatName;
  protected $formatInfo;

  public function __construct($name, $info) {
    $this->formatName = $name;
    $this->formatInfo = $info;
  }

  /**
   * Gets the representation of a resource.
   */
  public function viewResource($resourceController, $id) {
    $values = self::getData($resourceController->wrapper($id));
    return $this->serialize($values);
  }

  /**
   * Creates a new resource.
   */
  public function createResource($resourceController, $data) {
    $values = $this->unserialize($data);
    $id = $resourceController->create($values);
    $ref = self::getResourceReference($resourceController->resource(), $id);
    return $this->serialize($ref);
  }

  /**
   * Updates a resource.
   */
  public function updateResource($resourceController, $id, $data) {
    $values = $this->unserialize($data);
    $resourceController->update($id, $values);
  }

  /**
   * Deletes a resource.
   */
  public function deleteResource($resourceController, $id) {
    $resourceController->delete($id);
  }

  public function mimeType() {
    return $this->formatInfo['mime type'];
  }

  /**
   * Gets a simple PHP array using URI references for some wrapped data.
   */
  public static function getData($wrapper) {
    $data = array();
    foreach ($wrapper as $name => $property) {
      try {
        if ($property instanceof EntityDrupalWrapper) {
          // For referenced entities only return the URI.
          if ($id = $property->getIdentifier()) {
            $data[$name] = self::getResourceReference($property->type(), $id);
          }
        }
        elseif ($property instanceof EntityValueWrapper) {
          $data[$name] = $property->value();
        }
        elseif ($property instanceof EntityListWrapper || $property instanceof EntityStructureWrapper) {
          $data[$name] = self::getData($property);
        }
      }
      catch (EntityMetadataWrapperException $e) {
        // A property causes problems - ignore that.
      }
    }
    return $data;
  }

  public static function getResourceReference($resource, $id) {
    return array(
      'uri' => restws_resource_uri($resource, $id),
      'id' => $id,
      'resource' => $resource,
    );
  }
}

/**
 * A formatter to format json.
 */
class RestWSFormatJSON extends RestWSBaseFormat {

  public function serialize(array $values) {
    return drupal_json_encode($values);
  }

  public function unserialize($data) {
    return drupal_json_decode($data);
  }
}

/**
 * A formatter for XML.
 */
class RestWSFormatXML extends RestWSBaseFormat {

  /**
   * Gets the representation of a resource.
   */
  public function viewResource($resourceController, $id) {
    $xml = new DOMDocument('1.0', 'utf-8');
    $element = $xml->createElement($resourceController->resource());
    self::addToXML($xml, $element, $resourceController->wrapper($id));
    $xml->appendChild($element);
    return $xml->saveXML();
  }

  /**
   * Creates a new resource.
   */
  public function createResource($resourceController, $data) {
    $values = $this->unserialize($data);
    $id = $resourceController->create($values);

    $xml = new DOMDocument('1.0', 'utf-8');
    $element = $xml->createElement('uri');
    self::setXMLReference($element, $resourceController->resource(), $id);
    $xml->appendChild($element);
    return $xml->saveXML();
  }

  /**
   * Adds the data of the given wrapper to the given XML element.
   */
  public static function addToXML(DOMDocument $doc, DOMNode $parent, $wrapper) {
    foreach ($wrapper as $name => $property) {
      try {
        if ($property instanceof EntityDrupalWrapper) {
          // For referenced entities only return the URI.
          if ($id = $property->getIdentifier()) {
            $element = $doc->createElement($name);
            $parent->appendChild($element);
            self::setXMLReference($element, $property->type(), $id);
          }
        }
        elseif ($property instanceof EntityValueWrapper) {
          $element = $doc->createElement($name, $property->value());
          $parent->appendChild($element);
        }
        elseif ($property instanceof EntityListWrapper || $property instanceof EntityStructureWrapper) {
          $element = $doc->createElement(is_numeric($name) ? 'item' : $name);
          $parent->appendChild($element);
          self::addToXML($doc, $element, $property);
        }
      }
      catch (EntityMetadataWrapperException $e) {
        // A property causes problems - ignore that.
      }
    }
  }

  public static function setXMLReference(DOMElement $node, $resource, $id) {
    $node->nodeValue = restws_resource_uri($resource, $id);
    $node->setAttribute('resource', $resource);
    $node->setAttribute('id', $id);
  }
}

package com.srch2.android.http.service;

import java.util.HashSet;
import java.util.Iterator;
/**
 * Defines an index's structure such as its data fields. If an index is like an SQLite database table, the schema
 * is the table structure and its fields are the columns of that table. The fields of a schema must be named and
 * typed; they can also have additional properties such as highlighting and facet. See {@link com.srch2.android.http.service.Field}
 * for more information.
 * <br><br>
 * A schema <b>should only</b> be constructed when returning from the <code>Indexable</code> implementation of <code>getSchema()</code>.
 * A schema <b>should never</b> be initialized with null field values or duplicated fields.
 */
public final class Schema {

    final String uniqueKey;
    HashSet<Field> fields;
    boolean facetEnabled = false;
    int indexType = 0;

    Schema(PrimaryKeyField primaryKeyField, Field... remainingField) {
        if (primaryKeyField == null) {
            throw new IllegalArgumentException(
                    "Value of the field cannot be null");
        }

        uniqueKey = primaryKeyField.primaryKey.name;
        fields = new HashSet<Field>();
        fields.add(primaryKeyField.primaryKey);
        boolean searchableFieldExist = primaryKeyField.primaryKey.searchable;

        for (Field f : remainingField) {
            if (f == null) {
                throw new IllegalArgumentException(
                        "Value of the field cannot be null");
            }
            addToFields(f);
            if (!searchableFieldExist && f.searchable) {
                searchableFieldExist = true;
            }
        }
        if (!searchableFieldExist) {
            throw new IllegalArgumentException("No searchable field provided. The Engine need at least one searchable field to index on");
        }
    }

     /**
     * Defines the structure of an index's data fields. If an index is like a table in the SQLite database,
     * the schema is the table structure and its fields are the columns of that table. This method should
     * only be called when
     * returning the from the <code>Indexable</code> implementation of the method
     * <code>getSchema()</code>.
     * <br><br>
     * The first argument <b>should always</b> be the primary key field, which can be used to
     * retrieve a specific record or as the handle to delete the record from the index. Each
     * record <b>should have a unique value</b> for its primary key.
     * <br><br>
     * The remaining set of arguments are the rest of the schema's fields as they are defined
     * for the index. They can be passed in any order.
     * <br><br>
     * A schema initialized with null field values will cause an exception to be thrown when
     * <code>SRCH2Engine.initialize(Indexable firstIndex, Indexable... additionalIndexes)</code> is called.
     * @param primaryKeyField the field which will be the primary key of the index's schema
     * @param remainingField  the set of any other fields needed to define the schema
     */
    static public Schema createSchema(PrimaryKeyField primaryKeyField, Field... remainingField) {
        return new Schema(primaryKeyField, remainingField);
    }

    Schema(PrimaryKeyField primaryKeyField, String latitudeFieldName,
           String longitudeFieldName, Field... remainingField) {
        this(primaryKeyField, remainingField);
        addToFields(new Field(latitudeFieldName, Field.InternalType.LOCATION_LATITUDE,
                false, false, Field.DEFAULT_BOOST_VALUE));
        addToFields(new Field(longitudeFieldName,
                Field.InternalType.LOCATION_LONGITUDE, false, false,
                Field.DEFAULT_BOOST_VALUE));
        indexType = 1;
    }

    /**
     * Defines the structure of an index's data fields that is to include geosearch capability. If an index
     * is like a table in the SQLite database,
     * the schema is the table structure and its fields are the columns of that table. This method should
     * only be called when
     * returning the from the <code>Indexable</code> implementation of the method
     * <code>getSchema()</code>.
     * <br><br>
     * The first argument <b>should always</b> be the primary key field, which can be used to
     * retrieve a specific record or as the handle to delete the record from the index. Each
     * record <b>should have a unique value</b> for its primary key.
     * <br><br>
     * The second and fourth arguments <b>should always</b> be the latitude and longitude
     * fields, in that order, that are defined for the index's schema.
     * <br><br>
     * The remaining set of arguments are the rest of the schema's fields as they are defined
     * for the index. They can be passed in any order.
     * <br><br>
     * A schema initialized with null field values will cause an exception to be thrown when
     * <code>SRCH2Engine.initialize(Indexable firstIndex, Indexable... additionalIndexes)</code> is called.
     * @param primaryKeyField the field which will be the primary key of the index's schema
     * @param latitudeFieldName the field which will be the latitude field of the index's schema
     * @param longitudeFieldName the field which will be the longitude field of the index's schema
     * @param remainingField  the set of any other fields needed to define the schema
     */
    static public Schema createGeoSchema(PrimaryKeyField primaryKeyField, String latitudeFieldName,
                                  String longitudeFieldName, Field... remainingField) {
        return new Schema(primaryKeyField, latitudeFieldName,
                longitudeFieldName, remainingField);
    }





    private void addToFields(Field f) {
        if (fields.contains(f)) {
            throw new IllegalArgumentException("duplicated field:" + f.name);
        }
        fields.add(f);
    }


}

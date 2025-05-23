/**
 * map_reduce_replace.js
 *
 * Generates some random data and inserts it into a collection. Runs a
 * map-reduce command over the collection that computes the frequency
 * counts of the 'value' field and stores the results in an existing
 * collection.
 *
 * Uses the "replace" action to overwrite the entire contents of the
 * collection.
 * @tags: [
 *   # mapReduce does not support afterClusterTime.
 *   does_not_support_causal_consistency,
 *   # Use mapReduce.
 *   requires_scripting,
 *   # Disabled because MapReduce can lose cursors if the primary goes down during the operation.
 *   does_not_support_stepdowns,
 *   # TODO (SERVER-95170): Re-enable this test in txn suites.
 *   does_not_support_transactions,
 *   # TODO (SERVER-91002): server side javascript execution is deprecated, and the balancer is not
 *   # compatible with it, once the incompatibility is taken care off we can re-enable this test
 *   assumes_balancer_off
 * ]
 */
import {extendWorkload} from "jstests/concurrency/fsm_libs/extend_workload.js";
import {
    $config as $baseConfig
} from "jstests/concurrency/fsm_workloads/query/map_reduce/map_reduce_inline.js";

export const $config = extendWorkload($baseConfig, function($config, $super) {
    // Use the workload name as a prefix for the collection name,
    // since the workload name is assumed to be unique.
    var prefix = 'map_reduce_replace';

    function uniqueCollectionName(prefix, tid) {
        return prefix + tid;
    }

    $config.states.init = function init(db, collName) {
        $super.states.init.apply(this, arguments);

        this.outCollName = uniqueCollectionName(prefix, this.tid);
        assert.commandWorked(db.createCollection(this.outCollName));
    };

    $config.states.mapReduce = function mapReduce(db, collName) {
        var fullName = db[this.outCollName].getFullName();
        assert(db[this.outCollName].exists() !== null,
               "output collection '" + fullName + "' should exist");

        var options = {
            finalize: this.finalizer,
            out: {replace: this.outCollName},
            query: {key: {$exists: true}, value: {$exists: true}},
            sort: {_id: -1}  // sort key must be an existing index
        };

        var res = db[collName].mapReduce(this.mapper, this.reducer, options);
        assert.commandWorked(res);
    };

    return $config;
});

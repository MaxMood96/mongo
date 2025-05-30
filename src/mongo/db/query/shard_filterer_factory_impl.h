/**
 *    Copyright (C) 2020-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include "mongo/db/catalog/collection.h"
#include "mongo/db/exec/sbe/expressions/runtime_environment.h"
#include "mongo/db/exec/sbe/values/slot.h"
#include "mongo/db/exec/shard_filterer.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/query/multiple_collection_accessor.h"
#include "mongo/db/query/shard_filterer_factory_interface.h"

#include <memory>

namespace mongo {

/**
 * An implementation of ShardFiltererFactoryInterface.
 */
class ShardFiltererFactoryImpl : public ShardFiltererFactoryInterface {
public:
    ShardFiltererFactoryImpl(const CollectionPtr& collection) : _collection(collection) {}

    std::unique_ptr<ShardFilterer> makeShardFilterer(OperationContext* opCtx) const override;

private:
    const CollectionPtr& _collection;
};

/**
 * Construct a ShardFilterer for the main collection from 'collections' and populate the slot
 * specified by 'slotId' in 'env' with it.
 */
void populateShardFiltererSlot(OperationContext* opCtx,
                               sbe::RuntimeEnvironment& env,
                               sbe::value::SlotId shardFiltererSlot,
                               const MultipleCollectionAccessor& collections);

}  // namespace mongo

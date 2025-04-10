/**
 *    Copyright (C) 2024-present MongoDB, Inc.
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

#include "mongo/db/query/search/search_index_common.h"
#include "mongo/db/query/search/manage_search_index_request_gen.h"
#include "mongo/db/query/search/mongot_options.h"
#include "mongo/db/query/search/search_index_options.h"
#include "mongo/db/query/search/search_index_options_gen.h"
#include "mongo/db/query/search/search_index_process_interface.h"
#include "mongo/db/query/search/search_task_executors.h"
#include "mongo/db/version_context.h"
#include "mongo/rpc/get_status_from_command_result.h"

namespace mongo {
namespace {

/**
 * Takes the input for a ManageSearchIndexRequest and turns it into a RemoteCommandRequest targeting
 * the remote search index management endpoint.
 */
executor::RemoteCommandRequest createManageSearchIndexRemoteCommandRequest(
    OperationContext* opCtx,
    const NamespaceString& nss,
    const UUID& uuid,
    const BSONObj& userCmd,
    boost::optional<NamespaceString> viewName = boost::none,
    boost::optional<std::vector<BSONObj>> viewPipeline = boost::none) {
    // Fetch the search index management host and port.
    invariant(!globalSearchIndexParams.host.empty());
    auto swHostAndPort = HostAndPort::parse(globalSearchIndexParams.host);
    // This host and port string is configured and validated at startup.
    invariant(swHostAndPort.getStatus());

    // Format the command request.
    ManageSearchIndexRequest manageSearchIndexRequest;
    manageSearchIndexRequest.setManageSearchIndex(nss.coll());
    manageSearchIndexRequest.setCollectionUUID(uuid);
    manageSearchIndexRequest.setUserCommand(userCmd);
    if (viewName) {
        SearchIndexRequestView view;

        // Always append the view name.
        view.setName(viewName->coll());

        // Only append the view pipeline if it exists.
        // TODO SERVER-100553: replace this conditional with a tassert that viewPipeline exists.
        if (viewPipeline) {
            view.setEffectivePipeline(viewPipeline.value());
        }

        manageSearchIndexRequest.setView(view);
    }
    // Create a RemoteCommandRequest with the request and host-and-port.
    executor::RemoteCommandRequest remoteManageSearchIndexRequest(executor::RemoteCommandRequest(
        swHostAndPort.getValue(), nss.dbName(), manageSearchIndexRequest.toBSON(), opCtx));
    remoteManageSearchIndexRequest.sslMode = globalMongotParams.sslMode;
    return remoteManageSearchIndexRequest;
}
}  // namespace

std::tuple<const UUID,
           const NamespaceString,
           boost::optional<NamespaceString>,
           boost::optional<std::vector<BSONObj>>>
retrieveCollectionUUIDAndResolveViewOrThrow(OperationContext* opCtx,
                                            const NamespaceString& currentOperationNss) {
    // If the index management command is being run on a view, this call will return the
    // underlying source collection UUID and ResolvedView. If not, it will just return a UUID.
    auto collUUIDResolvedViewPair =
        SearchIndexProcessInterface::get(opCtx)->fetchCollectionUUIDAndResolveViewOrThrow(
            opCtx, currentOperationNss);
    // If the query is on a normal collection, the source collection will be the same as
    // the current NS.
    auto sourceCollectionNss = currentOperationNss;
    boost::optional<NamespaceString> viewNss;
    auto collUUID = collUUIDResolvedViewPair.first;
    boost::optional<std::vector<BSONObj>> viewPipeline;
    if (auto resolvedView = collUUIDResolvedViewPair.second) {
        uassert(
            ErrorCodes::QueryFeatureNotAllowed,
            "search index commands on views are not allowed in the current configuration. "
            "You may need to enable the "
            "correponding feature flag",
            feature_flags::gFeatureFlagMongotIndexedViews.isEnabledUseLatestFCVWhenUninitialized(
                VersionContext::getDecoration(opCtx),
                serverGlobalParams.featureCompatibility.acquireFCVSnapshot()));
        // The request is on a view! Therefore, currentOperationNss refers to the view
        // NS and the namespace on resolvedView refers to the underlying source collection.
        sourceCollectionNss = resolvedView.value().getNamespace();
        viewNss.emplace(currentOperationNss);

        viewPipeline.emplace(resolvedView.value().getPipeline());
    }

    return std::make_tuple(collUUID, sourceCollectionNss, viewNss, viewPipeline);
}

BSONObj getSearchIndexManagerResponse(OperationContext* opCtx,
                                      const NamespaceString& nss,
                                      const UUID& uuid,
                                      const BSONObj& userCmd,
                                      boost::optional<NamespaceString> viewName,
                                      boost::optional<std::vector<BSONObj>> viewPipeline) {
    // Create the RemoteCommandRequest.
    auto request = createManageSearchIndexRemoteCommandRequest(
        opCtx, nss, uuid, userCmd, viewName, viewPipeline);
    auto [promise, future] = makePromiseFuture<executor::TaskExecutor::RemoteCommandCallbackArgs>();
    auto promisePtr = std::make_shared<Promise<executor::TaskExecutor::RemoteCommandCallbackArgs>>(
        std::move(promise));

    // Schedule and run the RemoteCommandRequest on the TaskExecutor.
    auto taskExecutor = executor::getSearchIndexManagementTaskExecutor(opCtx->getServiceContext());
    auto scheduleResult = taskExecutor->scheduleRemoteCommand(
        std::move(request), [promisePtr](const auto& args) { promisePtr->emplaceValue(args); });
    if (!scheduleResult.isOK()) {
        // Since the command failed to be scheduled, the callback above did not and will not run.
        // Thus, it is safe to fulfill the promise here without worrying about synchronizing access
        // with the executor's thread.
        promisePtr->setError(scheduleResult.getStatus());
    }

    auto response = future.getNoThrow(opCtx);
    try {
        // Pull out the command response. Throw if the command did not reach the remote server.
        uassertStatusOK(response.getStatus());
        uassertStatusOK(response.getValue().response.status);
    } catch (const ExceptionFor<ErrorCodes::HostUnreachable>&) {
        // Don't expose the remote server host-and-port information to clients.
        // Also, change the error code to a non-retryable error code. A remote search index
        // management server instance is expected to be running on the same machine as the mongod.
        // Therefore, errors connecting with the search index server are not expected to change
        // without user intervention -- perhaps configuration changes.
        uasserted(ErrorCodes::CommandFailed,
                  "Error connecting to Search Index Management service.");
    }
    BSONObj responseData = response.getValue().response.data;

    // Check the command response for an error and throw if there is one.
    uassertStatusOK(getStatusFromCommandResult(responseData));
    // Return the successful command data to the caller.
    return responseData.getOwned();
}

/**
 * Check that the 'searchIndexManagementHostAndPort' server parameter has been set.
 * The search index commands are only allowed to run with external search index management.
 */
void throwIfNotRunningWithRemoteSearchIndexManagement() {
    auto& managementHost = globalSearchIndexParams.host;
    uassert(ErrorCodes::SearchNotEnabled,
            str::stream() << "Using Atlas Search Database Commands and the $listSearchIndexes "
                          << "aggregation stage requires additional configuration. Please connect "
                          << "to Atlas or an AtlasCLI local deployment to enable. For more "
                          << "information on how to connect, see "
                          << "https://dochub.mongodb.org/core/atlas-cli-deploy-local-reqs.",
            !managementHost.empty());
}


BSONObj runSearchIndexCommand(OperationContext* opCtx,
                              const NamespaceString& nss,
                              const BSONObj& cmdObj,
                              const UUID& collUUID,
                              boost::optional<NamespaceString> viewNss) {
    throwIfNotRunningWithRemoteSearchIndexManagement();

    BSONObj manageSearchIndexResponse =
        getSearchIndexManagerResponse(opCtx, nss, collUUID, cmdObj, viewNss);

    return manageSearchIndexResponse;
}
}  // namespace mongo

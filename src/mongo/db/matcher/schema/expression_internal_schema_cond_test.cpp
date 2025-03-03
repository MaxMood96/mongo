/**
 *    Copyright (C) 2018-present MongoDB, Inc.
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

#include <boost/move/utility_core.hpp>
#include <boost/optional/optional.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>

#include "mongo/base/status_with.h"
#include "mongo/bson/bsonmisc.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/bson/json.h"
#include "mongo/db/exec/matcher/matcher.h"
#include "mongo/db/matcher/expression_parser.h"
#include "mongo/db/matcher/schema/expression_internal_schema_cond.h"
#include "mongo/db/matcher/schema/expression_internal_schema_object_match.h"
#include "mongo/db/pipeline/expression_context.h"
#include "mongo/db/pipeline/expression_context_for_test.h"
#include "mongo/unittest/assert.h"
#include "mongo/unittest/framework.h"
#include "mongo/util/intrusive_counter.h"

namespace mongo {
namespace {
/**
 * Helper function for parsing and creating MatchExpressions.
 */
std::unique_ptr<InternalSchemaCondMatchExpression> createCondMatchExpression(BSONObj condition,
                                                                             BSONObj thenBranch,
                                                                             BSONObj elseBranch) {
    boost::intrusive_ptr<ExpressionContextForTest> expCtx(new ExpressionContextForTest());
    auto conditionExpr = MatchExpressionParser::parse(condition, expCtx);
    ASSERT_OK(conditionExpr.getStatus());
    auto thenBranchExpr = MatchExpressionParser::parse(thenBranch, expCtx);
    ASSERT_OK(thenBranchExpr.getStatus());
    auto elseBranchExpr = MatchExpressionParser::parse(elseBranch, expCtx);

    std::array<std::unique_ptr<MatchExpression>, 3> expressions = {
        {std::move(conditionExpr.getValue()),
         std::move(thenBranchExpr.getValue()),
         std::move(elseBranchExpr.getValue())}};

    auto cond = std::make_unique<InternalSchemaCondMatchExpression>(std::move(expressions));

    return cond;
}

TEST(InternalSchemaCondMatchExpressionTest, AcceptsObjectsThatMatch) {
    auto conditionQuery = BSON("age" << BSON("$lt" << 18));
    auto thenQuery = BSON("job"
                          << "student");
    auto elseQuery = BSON("job"
                          << "engineer");
    auto cond = createCondMatchExpression(conditionQuery, thenQuery, elseQuery);

    ASSERT_TRUE(exec::matcher::matchesBSON(cond.get(), fromjson("{age: 15, job: 'student'}")));
    ASSERT_TRUE(exec::matcher::matchesBSON(cond.get(), fromjson("{age: 18, job: 'engineer'}")));
    ASSERT_TRUE(
        exec::matcher::matchesBSON(cond.get(), fromjson("{age: [10, 20, 30], job: 'student'}")));
}

TEST(InternalSchemaCondMatchExpressionTest, RejectsObjectsThatDontMatch) {
    auto conditionQuery = BSON("age" << BSON("$lt" << 18));
    auto thenQuery = BSON("job"
                          << "student");
    auto elseQuery = BSON("job"
                          << "engineer");
    auto cond = createCondMatchExpression(conditionQuery, thenQuery, elseQuery);

    ASSERT_FALSE(exec::matcher::matchesBSON(cond.get(), fromjson("{age: 21, job: 'student'}")));
    ASSERT_FALSE(exec::matcher::matchesBSON(cond.get(), fromjson("{age: 5, job: 'engineer'}")));
    ASSERT_FALSE(exec::matcher::matchesBSON(cond.get(), fromjson("{age: 19}")));
    ASSERT_FALSE(exec::matcher::matchesBSON(cond.get(), fromjson("{age: 'blah'}")));
}

TEST(InternalSchemaCondMatchExpressionTest, EmptyMatchAlwaysUsesThenBranch) {
    auto conditionQuery = BSONObj();
    auto thenQuery = BSON("value" << BSON("$gte" << 0));
    auto elseQuery = BSON("value" << BSON("$lt" << 0));
    auto cond = createCondMatchExpression(conditionQuery, thenQuery, elseQuery);

    ASSERT_TRUE(exec::matcher::matchesBSON(cond.get(), BSON("value" << 0)));
    ASSERT_TRUE(exec::matcher::matchesBSON(cond.get(), BSON("value" << 2)));

    BSONObj match = BSON("value" << 10);
    ASSERT_TRUE(cond->matchesSingleElement(match.firstElement()));
}

TEST(InternalSchemaCondMatchExpressionTest, AppliesToSubobjectsViaObjectMatch) {
    auto conditionQuery = fromjson("{team: {$in: ['server', 'engineering']}}");
    auto thenQuery = BSON("subteam"
                          << "query");
    auto elseQuery = BSON("interests"
                          << "query optimization");

    InternalSchemaObjectMatchExpression objMatch(
        "job"_sd, createCondMatchExpression(conditionQuery, thenQuery, elseQuery));

    ASSERT_TRUE(exec::matcher::matchesBSON(
        &objMatch, fromjson("{name: 'anne', job: {team: 'engineering', subteam: 'query'}}")));
    ASSERT_TRUE(exec::matcher::matchesBSON(
        &objMatch, fromjson("{name: 'natalia', job: {team: 'server', subteam: 'query'}}")));
    ASSERT_TRUE(exec::matcher::matchesBSON(
        &objMatch,
        fromjson("{name: 'nicholas', job: {interests: ['query optimization', 'c++']}}")));

    ASSERT_FALSE(exec::matcher::matchesBSON(
        &objMatch, fromjson("{name: 'dave', team: 'server', subteam: 'query'}")));
    ASSERT_FALSE(exec::matcher::matchesBSON(
        &objMatch, fromjson("{name: 'mateo', interests: ['perl', 'python']}")));
    ASSERT_FALSE(exec::matcher::matchesBSON(
        &objMatch, fromjson("{name: 'lucas', job: {team: 'competitor', subteam: 'query'}}")));
    ASSERT_FALSE(exec::matcher::matchesBSON(
        &objMatch, fromjson("{name: 'marcos', job: {team: 'server', subteam: 'repl'}}")));
}

TEST(InternalSchemaCondMatchExpressionTest, EquivalentReturnsCorrectResults) {
    auto conditionQuery1 = BSON("foo" << 1);
    auto thenQuery1 = BSON("bar" << 2);
    auto elseQuery1 = BSON("qux" << 3);
    auto cond1 = createCondMatchExpression(conditionQuery1, thenQuery1, elseQuery1);

    auto conditionQuery2 = BSON("foo" << 1);
    auto thenQuery2 = BSON("bar" << 2);
    auto elseQuery2 = BSON("qux" << 3);
    auto cond2 = createCondMatchExpression(conditionQuery2, thenQuery2, elseQuery2);

    auto conditionQuery3 = BSON("foo" << 9001);
    auto thenQuery3 = BSON("bar" << 2);
    auto elseQuery3 = BSON("qux" << 3);
    auto cond3 = createCondMatchExpression(conditionQuery3, thenQuery3, elseQuery3);

    ASSERT_TRUE(cond1->equivalent(cond2.get()));
    ASSERT_TRUE(cond2->equivalent(cond1.get()));

    ASSERT_FALSE(cond1->equivalent(cond3.get()));
    ASSERT_FALSE(cond3->equivalent(cond2.get()));
}

TEST(InternalSchemaCondMatchExpressionTest, EquivalentToClone) {
    auto conditionQuery = BSON("likes"
                               << "cats");
    auto thenQuery = BSON("pets" << BSON("$lte" << 1));
    auto elseQuery = BSON("interests"
                          << "dogs");
    auto cond = createCondMatchExpression(conditionQuery, thenQuery, elseQuery);
    auto clone = cond->clone();
    ASSERT_TRUE(cond->equivalent(clone.get()));
}
}  // namespace
}  // namespace mongo

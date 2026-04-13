/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "velox/experimental/cudf/expression/ExpressionTreeCompiler.h"
#include "velox/experimental/cudf/expression/ExpressionEvaluatorRegistry.h"

namespace facebook::velox::cudf_velox {

namespace {

/// Recursively walk through all children that the given evaluator handles
/// natively. For each child the evaluator cannot handle (a "boundary"),
/// recursively compile it and store the result in `resolved`.
void resolveBoundaries(
    const core::TypedExprPtr& expr,
    const CudfExpressionEvaluatorEntry& evaluator,
    const RowTypePtr& inputRowSchema,
    ResolvedSubExprs& resolved) {
  for (const auto& child : expr->inputs()) {
    // Leaf nodes — every evaluator handles these internally.
    if (child->isConstantKind() || child->isInputKind()) {
      continue;
    }
    if (evaluator.canEvaluate && evaluator.canEvaluate(child)) {
      // Evaluator handles this child natively. Recurse to find deeper
      // boundaries.
      resolveBoundaries(child, evaluator, inputRowSchema, resolved);
    } else {
      // Boundary: this child needs a different evaluator.
      // Recursively compile it (the full compile call picks the best
      // evaluator for this sub-tree).
      resolved[child.get()] =
          ExpressionTreeCompiler::compile(child, inputRowSchema);
      // Don't recurse into boundary — compile() handles the entire subtree.
    }
  }
}

} // namespace

std::shared_ptr<CudfExpression> ExpressionTreeCompiler::compile(
    const core::TypedExprPtr& expr,
    const RowTypePtr& inputRowSchema) {
  ResolvedSubExprs resolved;
  return compile(expr, inputRowSchema, resolved);
}

std::shared_ptr<CudfExpression> ExpressionTreeCompiler::compile(
    const core::TypedExprPtr& expr,
    const RowTypePtr& inputRowSchema,
    ResolvedSubExprs& resolvedOut) {
  ensureBuiltinExpressionEvaluatorsRegistered();

  // Step 1: Select the best evaluator for the root expression.
  const auto* best = findBestEvaluator(expr);
  VELOX_CHECK_NOT_NULL(
      best,
      "No cuDF expression evaluator can handle: {}",
      expr->toString());

  // Step 2: Walk children to find boundaries and recursively compile them.
  resolveBoundaries(expr, *best, inputRowSchema, resolvedOut);

  // Step 3: Create the root evaluator with pre-resolved sub-expressions.
  return best->create(expr, inputRowSchema, resolvedOut);
}

} // namespace facebook::velox::cudf_velox

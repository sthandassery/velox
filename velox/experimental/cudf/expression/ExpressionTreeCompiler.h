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

#pragma once

#include "velox/experimental/cudf/expression/ExpressionEvaluator.h"

namespace facebook::velox::cudf_velox {

/// A map from sub-expression pointer to a pre-built CudfExpression.
/// Keyed by raw TypedExpr pointer for identity-based matching within a single
/// compilation pass.
using ResolvedSubExprs =
    std::unordered_map<const core::ITypedExpr*, std::shared_ptr<CudfExpression>>;

/// Compiles a TypedExpr tree into a CudfExpression by selecting the best
/// evaluator for each part of the tree. All evaluator selection logic is
/// centralized here — individual evaluators never query the registry.
///
/// The compiler walks the expression tree top-down:
///   1. Select the best evaluator for the root expression.
///   2. Walk children: if the selected evaluator can handle a child natively,
///      recurse into it; otherwise, the child is a "boundary" that needs
///      a different evaluator.
///   3. Recursively compile each boundary sub-expression (selecting its own
///      evaluator).
///   4. Create the root evaluator, passing it the pre-resolved map of
///      boundary sub-expressions → CudfExpression.
class ExpressionTreeCompiler {
 public:
  /// Compile an expression tree into a fully-wired CudfExpression.
  static std::shared_ptr<CudfExpression> compile(
      const core::TypedExprPtr& expr,
      const RowTypePtr& inputRowSchema);

  /// Compile, and also expose the resolved boundary sub-expressions.
  /// Callers that build their own AST tree (e.g. hash-join with two-table
  /// schema) need the resolved map to pass to createAstTree().
  static std::shared_ptr<CudfExpression> compile(
      const core::TypedExprPtr& expr,
      const RowTypePtr& inputRowSchema,
      ResolvedSubExprs& resolvedOut);
};

} // namespace facebook::velox::cudf_velox

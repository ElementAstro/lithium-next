"""
Lithium export declarations for executor module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from typing import Any, Dict

from lithium_bridge import expose_command, expose_controller


@expose_controller(
    endpoint="/api/executor/run",
    method="POST",
    description="Execute Python code in isolated environment",
    tags=["executor", "python"],
    version="1.0.0",
)
def execute_code(
    code: str,
    timeout: int = 30,
    memory_limit: int = 256,
    globals_dict: Dict[str, Any] = None,
) -> dict:
    """
    Execute Python code in an isolated environment.

    Args:
        code: Python code to execute
        timeout: Execution timeout in seconds
        memory_limit: Memory limit in MB
        globals_dict: Global variables to pass to the code

    Returns:
        Dictionary with execution result
    """
    from .isolated_executor import IsolatedExecutor

    try:
        executor = IsolatedExecutor(
            timeout=timeout,
            memory_limit_mb=memory_limit,
        )
        result = executor.execute(code, globals_dict or {})
        return {
            "success": result.success,
            "output": result.output,
            "error": result.error,
            "execution_time": result.execution_time,
        }
    except Exception as e:
        return {
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/executor/eval",
    method="POST",
    description="Evaluate a Python expression",
    tags=["executor", "python"],
    version="1.0.0",
)
def evaluate_expression(
    expression: str,
    timeout: int = 10,
    locals_dict: Dict[str, Any] = None,
) -> dict:
    """
    Evaluate a Python expression.

    Args:
        expression: Python expression to evaluate
        timeout: Evaluation timeout in seconds
        locals_dict: Local variables for evaluation

    Returns:
        Dictionary with evaluation result
    """
    from .isolated_executor import IsolatedExecutor

    try:
        executor = IsolatedExecutor(timeout=timeout)
        result = executor.evaluate(expression, locals_dict or {})
        return {
            "success": True,
            "result": str(result),
            "type": type(result).__name__,
        }
    except Exception as e:
        return {
            "success": False,
            "error": str(e),
        }


@expose_command(
    command_id="executor.run",
    description="Execute Python code (command)",
    priority=3,
    timeout_ms=60000,
    tags=["executor"],
)
def cmd_execute_code(code: str, timeout: int = 30) -> dict:
    """Execute Python code via command dispatcher."""
    return execute_code(code, timeout)


@expose_command(
    command_id="executor.eval",
    description="Evaluate Python expression (command)",
    priority=5,
    timeout_ms=15000,
    tags=["executor"],
)
def cmd_evaluate(expression: str) -> dict:
    """Evaluate expression via command dispatcher."""
    return evaluate_expression(expression)

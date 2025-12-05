import json
from typing import List
from client.gcs import PathTuple, PathRef, PathArray, PathLeaf, PathLeafType


def is_dynamic_type(param_type: str, param: dict) -> bool:
    """Check if a parameter type is dynamic (needs offset/reference)."""
    # Dynamic arrays
    if param_type.endswith('[]'):
        return True

    # Dynamic bytes and string
    if param_type in ['string', 'bytes']:
        return True

    # Tuples containing dynamic types
    if 'components' in param:
        return any(is_dynamic_type(c['type'], c) for c in param['components'])

    return False


def get_function_params(abi_filename: str, function_name: str) -> List[str]:
    """
    Get list of parameter names for a function.

    Args:
        abi_filename: Path to ABI JSON file
        function_name: Name of the function

    Returns:
        List of parameter names
    """
    with open(abi_filename, 'r', encoding='utf-8') as f:
        abi = json.load(f)

    # Find the function in ABI
    for item in abi:
        if item.get('type') == 'function' and item.get('name') == function_name:
            return [p.get('name', f'param_{i}') for i, p in enumerate(item.get('inputs', []))]

    raise ValueError(f"Function '{function_name}' not found in ABI")


def get_tuple_fields(abi_filename: str, function_name: str, tuple_param: str) -> List[str]:
    """
    Get list of field names inside a tuple parameter.

    Args:
        abi_filename: Path to ABI JSON file
        function_name: Name of the function
        tuple_param: Name of the tuple parameter

    Returns:
        List of field names in the tuple
    """
    with open(abi_filename, 'r', encoding='utf-8') as f:
        abi = json.load(f)

    # Find the function
    for item in abi:
        if item.get('type') == 'function' and item.get('name') == function_name:
            # Find the tuple parameter
            for param in item.get('inputs', []):
                if param.get('name') == tuple_param:
                    if 'components' not in param:
                        raise ValueError(f"Parameter '{tuple_param}' is not a tuple")
                    return [c.get('name', f'field_{i}') for i, c in enumerate(param['components'])]

            raise ValueError(f"Tuple parameter '{tuple_param}' not found")

    raise ValueError(f"Function '{function_name}' not found in ABI")


def get_path(abi_filename: str, function_name: str, param_name: str) -> List:
    """
    Get the DataPath components for a specific parameter.

    Args:
        abi_filename: Path to ABI JSON file
        function_name: Name of the function
        param_name: Name of the parameter to find

    Returns:
        List of DataPath components [PathTuple, PathRef, etc.]
    """
    with open(abi_filename, 'r', encoding='utf-8') as f:
        abi = json.load(f)

    # Find the function in ABI
    function_abi = None
    for item in abi:
        if item.get('type') == 'function' and item.get('name') == function_name:
            function_abi = item
            break

    if not function_abi:
        raise ValueError(f"Function '{function_name}' not found in ABI")

    # Find the parameter
    for idx, param in enumerate(function_abi['inputs']):
        if param.get('name') == param_name:
            return build_path(param, idx)

    # If not found, list available parameters for debugging
    available = [p.get('name', f'param_{i}') for i, p in enumerate(function_abi['inputs'])]
    raise ValueError(
        f"Parameter '{param_name}' not found in function '{function_name}'. "
        f"Available parameters: {', '.join(available)}"
    )


def build_path(param: dict, tuple_index: int, parent_path: List = None) -> List:
    """
    Build the path components for a parameter.

    Returns a list of GCS path objects ready to use in DataPath().
    """
    if parent_path is None:
        parent_path = []

    path = parent_path + [PathTuple(tuple_index)]
    param_type = param['type']

    # Check if the parameter is dynamic
    if is_dynamic_type(param_type, param):
        path.append(PathRef())

        # Handle arrays
        if '[]' in param_type:
            path.append(PathArray(1))  # Default weight=1

            # Determine the element type (remove the [])
            element_type = param_type.rstrip('[]')

            # Check if array elements are dynamic
            # For primitive types like uint256[], the elements are static
            if element_type in ['string', 'bytes'] or 'tuple' in element_type:
                # Elements themselves are dynamic
                path.append(PathLeaf(PathLeafType.DYNAMIC))
            else:
                # Elements are static (uint256, address, etc.)
                path.append(PathLeaf(PathLeafType.STATIC))
        elif param_type in ['string', 'bytes']:
            # Dynamic bytes/string
            path.append(PathLeaf(PathLeafType.DYNAMIC))
        else:
            # Dynamic tuple
            path.append(PathLeaf(PathLeafType.STATIC))
    else:
        # Static type - direct leaf
        path.append(PathLeaf(PathLeafType.STATIC))

    return path


def get_nested_path(parent_abi: str,
                    parent_function: str,
                    parent_param: str,
                    nested_abi: str,
                    nested_function: str,
                    nested_param: str) -> List:
    """
    Get the DataPath for a nested parameter inside a calldata field.

    Example:
        Safe.execTransaction() has a 'data' parameter containing addOwnerWithThreshold()
        To get the path to 'owner' inside addOwnerWithThreshold:

        get_nested_path(
            "abis/safe_1.4.1.abi.json", "execTransaction", "data",
            "abis/safe_1.4.1.abi.json", "addOwnerWithThreshold", "owner"
        )
    """
    # Get parent path (to the calldata field)
    parent_path = get_path(parent_abi, parent_function, parent_param)

    # Remove the final PathLeaf from parent (we'll go deeper)
    parent_path_clean = [p for p in parent_path if not isinstance(p, PathLeaf)]

    # Get nested path
    nested_path = get_path(nested_abi, nested_function, nested_param)

    # The nested path starts at offset 0 within the calldata, so we keep its PathTuple
    # but it's relative to the parent's data field
    return parent_path_clean + nested_path


def get_path_in_array(abi_filename: str,
                      function_name: str,
                      array_param: str,
                      element_index: int = None) -> List:
    """
    Get the DataPath for an element inside an array parameter.

    Args:
        abi_filename: Path to ABI JSON file
        function_name: Name of the function
        array_param: Name of the array parameter
        element_index: Specific index in array, or None for all elements

    Returns:
        List of DataPath components
    """
    path = get_path(abi_filename, function_name, array_param)

    # The path should already have PathRef() and PathArray()
    # We just need to specify the element if needed
    if element_index is not None:
        # Replace generic PathArray with indexed one
        for component in path:
            if isinstance(component, PathArray):
                # For specific index, we might need different handling
                # depending on your GCS implementation
                pass

    return path


def get_path_in_tuple(abi_filename: str,
                      function_name: str,
                      tuple_param: str,
                      field_name: str) -> List:
    """
    Get the DataPath for a field inside a tuple parameter.

    Args:
        abi_filename: Path to ABI JSON file
        function_name: Name of the function
        tuple_param: Name of the tuple parameter
        field_name: Name of the field in the tuple (or index as string)

    Returns:
        List of DataPath components
    """
    with open(abi_filename, 'r', encoding='utf-8') as f:
        abi = json.load(f)

    # Find the function
    function_abi = None
    for item in abi:
        if item.get('type') == 'function' and item.get('name') == function_name:
            function_abi = item
            break

    if not function_abi:
        raise ValueError(f"Function '{function_name}' not found")

    # Find the tuple parameter
    for idx, param in enumerate(function_abi['inputs']):
        if param.get('name') == tuple_param:
            if 'components' not in param:
                raise ValueError(f"Parameter '{tuple_param}' is not a tuple")

            # Build path to the tuple parameter itself
            base_path = build_path(param, idx)

            # Remove the final leaf (we're going deeper into the tuple)
            base_path = [p for p in base_path if not isinstance(p, PathLeaf)]

            # Find the field in the tuple components
            for field_idx, component in enumerate(param['components']):
                if component.get('name') == field_name:
                    # Add path to the specific field
                    field_path = [PathTuple(field_idx)]

                    # Add final leaf based on field type
                    if is_dynamic_type(component['type'], component):
                        field_path.extend([PathRef(), PathLeaf(PathLeafType.DYNAMIC)])
                    else:
                        field_path.append(PathLeaf(PathLeafType.STATIC))

                    return base_path + field_path

            # List available fields for debugging
            available = [c.get('name', f'field_{i}') for i, c in enumerate(param['components'])]
            raise ValueError(
                f"Field '{field_name}' not found in tuple '{tuple_param}'. "
                f"Available fields: {', '.join(available)}"
            )

    raise ValueError(f"Tuple parameter '{tuple_param}' not found")


def get_all_paths(abi_filename: str, function_name: str) -> dict[str, List]:
    """
    Get a dictionary mapping parameter names to their DataPath components.

    Args:
        abi_filename: Path to ABI JSON file
        function_name: Name of the function

    Returns:
        Dictionary with parameter names as keys and path lists as values
    """
    params = get_function_params(abi_filename, function_name)
    return {param: get_path(abi_filename, function_name, param) for param in params}


def get_all_tuple_paths(abi_filename: str, function_name: str, tuple_param: str) -> dict[str, List]:
    """
    Get a dictionary mapping tuple field names to their DataPath components.

    Args:
        abi_filename: Path to ABI JSON file
        function_name: Name of the function
        tuple_param: Name of the tuple parameter

    Returns:
        Dictionary with field names as keys and path lists as values
    """
    fields = get_tuple_fields(abi_filename, function_name, tuple_param)
    return {field: get_path_in_tuple(abi_filename, function_name, tuple_param, field) for field in fields}


def get_all_tuple_array_paths(abi_filename: str, function_name: str, array_param: str) -> dict[str, List]:
    """
    Get paths for fields inside a tuple that is in an array.

    For example: batchExecute has a parameter 'calls' which is an array of tuples.
    Each tuple has fields 'to', 'value', 'data'.

    This function returns the complete path including the array navigation.

    Args:
        abi_filename: Path to ABI JSON file
        function_name: Name of the function
        array_param: Name of the array parameter (containing tuples)

    Returns:
        Dictionary with field names as keys and complete path lists as values
    """
    with open(abi_filename, 'r', encoding='utf-8') as f:
        abi = json.load(f)

    # Find the function
    function_abi = None
    for item in abi:
        if item.get('type') == 'function' and item.get('name') == function_name:
            function_abi = item
            break

    if not function_abi:
        raise ValueError(f"Function '{function_name}' not found")

    # Find the array parameter
    for idx, param in enumerate(function_abi['inputs']):
        if param.get('name') == array_param:
            if not param['type'].endswith('[]'):
                raise ValueError(f"Parameter '{array_param}' is not an array")

            if 'components' not in param:
                raise ValueError(f"Array parameter '{array_param}' does not contain tuples")

            # Build base path to the array
            base_path = [PathTuple(idx), PathRef(), PathArray(1)]

            # If the tuple itself is dynamic, add another PathRef
            # (this happens when the tuple contains dynamic types)
            if is_dynamic_type(param['type'].rstrip('[]'), param):
                base_path.append(PathRef())

            # Get paths for each field in the tuple
            result = {}
            for field_idx, component in enumerate(param['components']):
                field_name = component.get('name', f'field_{field_idx}')

                # Add the tuple field index
                field_path = base_path + [PathTuple(field_idx)]

                # Add final leaf based on field type
                if is_dynamic_type(component['type'], component):
                    field_path.extend([PathRef(), PathLeaf(PathLeafType.DYNAMIC)])
                else:
                    field_path.append(PathLeaf(PathLeafType.STATIC))

                result[field_name] = field_path

            return result

    raise ValueError(f"Array parameter '{array_param}' not found")


# Helper to print the path in a readable format
def print_path(path: List) -> None:
    """Convert path components to readable string."""
    components = []
    for p in path:
        if isinstance(p, PathTuple):
            components.append(f"PathTuple({p.value})")
        elif isinstance(p, PathRef):
            components.append("PathRef()")
        elif isinstance(p, PathArray):
            components.append(f"PathArray({p.weight})")
        elif isinstance(p, PathLeaf):
            components.append(f"PathLeaf(PathLeafType.{p.type.name})")
    result = "[\n            " + ",\n            ".join(components) + ",\n        ]"
    print(result)


def print_all_paths(param_paths: dict[str, List]) -> None:
    """Print all the paths in a readable format."""
    for param_name, path in param_paths.items():
        print(f"path for {param_name}:")
        print_path(path)
        print("---")


# Example usage
if __name__ == "__main__":
    print("# Example 1: Simple ERC20 transfer")
    print("# Get path for '_to' parameter:")
    ex_path = get_path("abis/erc20.json", "transfer", "_to")
    print_path(ex_path)

    print("\n# Example 2: Nested calldata")
    print("# Safe execTransaction -> addOwnerWithThreshold -> owner")
    ex_path = get_nested_path(
        "abis/safe_1.4.1.abi.json", "execTransaction", "data",
        "abis/safe_1.4.1.abi.json", "addOwnerWithThreshold", "owner"
    )
    print_path(ex_path)

    print("\n# Example 3: Tuple field in batch.json")
    print("# batchExecute -> calls[].to")
    ex_path = get_path_in_tuple("abis/batch.json", "batchExecute", "calls", "to")
    print_path(ex_path)

    print("\n# Example 4: Array element")
    print("# ERC1155 safeBatchTransferFrom -> _ids[]")
    ex_path = get_path("abis/erc1155.json", "safeBatchTransferFrom", "_ids")
    print_path(ex_path)

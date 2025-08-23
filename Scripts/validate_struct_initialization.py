#!/usr/bin/env python3
"""
Static Analysis Validation for UPROPERTY FGuid Initialization Patterns

This script scans C++ header files for UPROPERTY FGuid patterns and validates
that they follow proper initialization patterns as established in the struct
initialization fix.

Requirements:
- All UPROPERTY FGuid members must have proper in-class initializers
- Supports suppression file for legitimate exceptions
- Generates detailed report of validation results
"""

import os
import re
import sys
import json
import argparse
from pathlib import Path
from typing import List, Dict, Set, Tuple, Optional
from dataclasses import dataclass


@dataclass
class FGuidProperty:
    """Represents a found UPROPERTY FGuid member"""
    file_path: str
    line_number: int
    struct_name: str
    property_name: str
    uproperty_line: str
    declaration_line: str
    has_initializer: bool
    initializer_value: Optional[str] = None


@dataclass
class ValidationResult:
    """Results of validation for a single file"""
    file_path: str
    properties_found: List[FGuidProperty]
    valid_properties: List[FGuidProperty]
    invalid_properties: List[FGuidProperty]
    suppressed_properties: List[FGuidProperty]


class StructInitializationValidator:
    """Main validator class for UPROPERTY FGuid patterns"""
    
    def __init__(self, suppression_file: Optional[str] = None):
        self.suppression_patterns: Set[str] = set()
        self.load_suppressions(suppression_file)
        
        # Regex patterns for detection
        self.uproperty_pattern = re.compile(
            r'^\s*UPROPERTY\s*\([^)]*\)\s*$',
            re.MULTILINE
        )
        
        self.fguid_declaration_pattern = re.compile(
            r'^\s*FGuid\s+(\w+)(?:\s*=\s*([^;]+))?\s*;\s*(?://.*)?$',
            re.MULTILINE
        )
        
        self.struct_pattern = re.compile(
            r'^\s*(?:USTRUCT\s*\([^)]*\)\s*)?struct\s+(?:VIBEHEIM_API\s+)?(\w+)',
            re.MULTILINE
        )
        
        # Valid initialization patterns
        self.valid_initializers = {
            'FGuid()',
            'FGuid::NewGuid()',
            'FGuid::NewGuid()',  # Handle potential whitespace variations
        }
    
    def load_suppressions(self, suppression_file: Optional[str]) -> None:
        """Load suppression patterns from file"""
        if not suppression_file or not os.path.exists(suppression_file):
            return
            
        try:
            with open(suppression_file, 'r') as f:
                data = json.load(f)
                self.suppression_patterns = set(data.get('suppressions', []))
        except (json.JSONDecodeError, IOError) as e:
            print(f"Warning: Could not load suppression file {suppression_file}: {e}")
    
    def is_suppressed(self, file_path: str, struct_name: str, property_name: str) -> bool:
        """Check if a property is suppressed"""
        patterns_to_check = [
            f"{file_path}:{struct_name}::{property_name}",
            f"{struct_name}::{property_name}",
            f"*::{property_name}",
            file_path,
        ]
        
        return any(pattern in self.suppression_patterns for pattern in patterns_to_check)
    
    def find_struct_name(self, content: str, line_number: int) -> str:
        """Find the struct name that contains the given line"""
        lines = content.split('\n')
        
        # Search backwards from the property line to find the struct declaration
        for i in range(line_number - 1, -1, -1):
            line = lines[i]
            match = self.struct_pattern.search(line)
            if match:
                return match.group(1)
        
        return "UnknownStruct"
    
    def validate_file(self, file_path: str) -> ValidationResult:
        """Validate a single header file"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except (IOError, UnicodeDecodeError) as e:
            print(f"Warning: Could not read file {file_path}: {e}")
            return ValidationResult(file_path, [], [], [], [])
        
        properties_found = []
        lines = content.split('\n')
        
        # Find all UPROPERTY declarations followed by FGuid
        i = 0
        while i < len(lines):
            line = lines[i]
            
            # Check if this line contains UPROPERTY
            if self.uproperty_pattern.search(line):
                # Look for FGuid declaration in the next few lines
                for j in range(i + 1, min(i + 5, len(lines))):
                    next_line = lines[j]
                    fguid_match = self.fguid_declaration_pattern.search(next_line)
                    
                    if fguid_match:
                        property_name = fguid_match.group(1)
                        initializer = fguid_match.group(2)
                        struct_name = self.find_struct_name(content, j + 1)
                        
                        has_initializer = initializer is not None
                        initializer_value = initializer.strip() if initializer else None
                        
                        prop = FGuidProperty(
                            file_path=file_path,
                            line_number=j + 1,
                            struct_name=struct_name,
                            property_name=property_name,
                            uproperty_line=line.strip(),
                            declaration_line=next_line.strip(),
                            has_initializer=has_initializer,
                            initializer_value=initializer_value
                        )
                        
                        properties_found.append(prop)
                        break
            
            i += 1
        
        # Categorize properties
        valid_properties = []
        invalid_properties = []
        suppressed_properties = []
        
        for prop in properties_found:
            if self.is_suppressed(file_path, prop.struct_name, prop.property_name):
                suppressed_properties.append(prop)
            elif self.is_valid_initialization(prop):
                valid_properties.append(prop)
            else:
                invalid_properties.append(prop)
        
        return ValidationResult(
            file_path=file_path,
            properties_found=properties_found,
            valid_properties=valid_properties,
            invalid_properties=invalid_properties,
            suppressed_properties=suppressed_properties
        )
    
    def is_valid_initialization(self, prop: FGuidProperty) -> bool:
        """Check if a property has valid initialization"""
        if not prop.has_initializer:
            return False
        
        # Normalize the initializer value
        normalized = prop.initializer_value.strip()
        
        # Check against known valid patterns
        return normalized in self.valid_initializers
    
    def scan_directory(self, directory: str, recursive: bool = True) -> List[ValidationResult]:
        """Scan directory for header files and validate them"""
        results = []
        
        if recursive:
            pattern = "**/*.h"
        else:
            pattern = "*.h"
        
        for header_file in Path(directory).glob(pattern):
            if header_file.is_file():
                result = self.validate_file(str(header_file))
                if result.properties_found:  # Only include files with FGuid properties
                    results.append(result)
        
        return results
    
    def generate_report(self, results: List[ValidationResult]) -> str:
        """Generate a detailed validation report"""
        total_properties = sum(len(r.properties_found) for r in results)
        total_valid = sum(len(r.valid_properties) for r in results)
        total_invalid = sum(len(r.invalid_properties) for r in results)
        total_suppressed = sum(len(r.suppressed_properties) for r in results)
        
        report = []
        report.append("=" * 80)
        report.append("UPROPERTY FGuid Initialization Validation Report")
        report.append("=" * 80)
        report.append("")
        report.append(f"Total files scanned: {len(results)}")
        report.append(f"Total UPROPERTY FGuid properties found: {total_properties}")
        report.append(f"Valid properties: {total_valid}")
        report.append(f"Invalid properties: {total_invalid}")
        report.append(f"Suppressed properties: {total_suppressed}")
        report.append("")
        
        if total_invalid > 0:
            report.append("❌ VALIDATION FAILED")
            report.append("")
            report.append("Invalid Properties (require fixes):")
            report.append("-" * 40)
            
            for result in results:
                for prop in result.invalid_properties:
                    report.append(f"File: {prop.file_path}:{prop.line_number}")
                    report.append(f"Struct: {prop.struct_name}")
                    report.append(f"Property: {prop.property_name}")
                    report.append(f"Declaration: {prop.declaration_line}")
                    if prop.has_initializer:
                        report.append(f"Current initializer: {prop.initializer_value}")
                        report.append("Issue: Invalid initializer pattern")
                    else:
                        report.append("Issue: Missing in-class initializer")
                    report.append("Expected: FGuid() or FGuid::NewGuid()")
                    report.append("")
        else:
            report.append("✅ VALIDATION PASSED")
            report.append("")
        
        if total_suppressed > 0:
            report.append("Suppressed Properties:")
            report.append("-" * 20)
            for result in results:
                for prop in result.suppressed_properties:
                    report.append(f"{prop.file_path}:{prop.line_number} - {prop.struct_name}::{prop.property_name}")
            report.append("")
        
        if total_valid > 0:
            report.append("Valid Properties:")
            report.append("-" * 15)
            for result in results:
                for prop in result.valid_properties:
                    report.append(f"✅ {prop.file_path}:{prop.line_number} - {prop.struct_name}::{prop.property_name} = {prop.initializer_value}")
            report.append("")
        
        return "\n".join(report)


def create_default_suppression_file(file_path: str) -> None:
    """Create a default suppression file with examples"""
    default_suppressions = {
        "suppressions": [
            "# Example suppressions:",
            "# \"Source/Vibeheim/WorldGen/Public/Data/LegacyStruct.h:FLegacyStruct::LegacyId\"",
            "# \"FTemporaryStruct::TempId\"",
            "# \"*::DebugId\""
        ]
    }
    
    with open(file_path, 'w') as f:
        json.dump(default_suppressions, f, indent=2)


def main():
    parser = argparse.ArgumentParser(
        description="Validate UPROPERTY FGuid initialization patterns"
    )
    parser.add_argument(
        "directory",
        help="Directory to scan for header files"
    )
    parser.add_argument(
        "--recursive", "-r",
        action="store_true",
        default=True,
        help="Scan directory recursively (default: True)"
    )
    parser.add_argument(
        "--suppression-file", "-s",
        help="Path to suppression file (JSON format)"
    )
    parser.add_argument(
        "--create-suppression-file",
        help="Create a default suppression file at the specified path"
    )
    parser.add_argument(
        "--output", "-o",
        help="Output file for the report (default: stdout)"
    )
    parser.add_argument(
        "--fail-on-invalid",
        action="store_true",
        help="Exit with non-zero code if invalid properties are found"
    )
    
    args = parser.parse_args()
    
    # Create suppression file if requested
    if args.create_suppression_file:
        create_default_suppression_file(args.create_suppression_file)
        print(f"Created default suppression file: {args.create_suppression_file}")
        return 0
    
    # Validate directory exists
    if not os.path.isdir(args.directory):
        print(f"Error: Directory '{args.directory}' does not exist")
        return 1
    
    # Run validation
    validator = StructInitializationValidator(args.suppression_file)
    results = validator.scan_directory(args.directory, args.recursive)
    
    # Generate report
    report = validator.generate_report(results)
    
    # Output report
    if args.output:
        with open(args.output, 'w') as f:
            f.write(report)
        print(f"Report written to: {args.output}")
    else:
        print(report)
    
    # Check for failures
    total_invalid = sum(len(r.invalid_properties) for r in results)
    
    if args.fail_on_invalid and total_invalid > 0:
        return 1
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
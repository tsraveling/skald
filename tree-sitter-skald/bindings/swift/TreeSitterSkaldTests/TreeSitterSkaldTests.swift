import XCTest
import SwiftTreeSitter
import TreeSitterSkald

final class TreeSitterSkaldTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_skald())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading Skald grammar")
    }
}
